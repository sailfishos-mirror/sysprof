#include "config.h"

#include <sysprof-analyze.h>

#include "sysprof-document-private.h"

static GMainLoop *main_loop;

static void
load_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  SysprofDocumentLoader *loader = (SysprofDocumentLoader *)object;
  g_autoptr(SysprofDocument) document = NULL;
  g_autoptr(GListModel) traceables = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GString) str = NULL;
  SysprofSymbol *symbols[128];
  guint n_symbols;
  guint n_items;

  g_assert (SYSPROF_IS_DOCUMENT_LOADER (loader));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(document = sysprof_document_loader_load_finish (loader, result, &error)))
    g_error ("Failed to load document: %s", error->message);

  traceables = sysprof_document_list_traceables (document);
  n_items = g_list_model_get_n_items (traceables);

  str = g_string_new ("");

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(SysprofDocumentTraceable) traceable = g_list_model_get_item (traceables, i);

      str->len = 0;
      str->str[0] = 0;

      g_assert (traceable != NULL);
      g_assert (SYSPROF_IS_DOCUMENT_TRACEABLE (traceable));

      n_symbols = sysprof_document_symbolize_traceable (document,
                                                        traceable,
                                                        symbols,
                                                        G_N_ELEMENTS (symbols));

      g_print ("%s depth=%u pid=%u\n",
               G_OBJECT_TYPE_NAME (traceable),
               n_symbols,
               sysprof_document_frame_get_pid (SYSPROF_DOCUMENT_FRAME (traceable)));

      for (guint j = 0; j < n_symbols; j++)
        {
          SysprofSymbol *symbol = symbols[j];
          const char *name;
          const char *path;
          const char *nick;

          if (symbol != NULL)
            {
              name = sysprof_symbol_get_name (symbol);
              path = sysprof_symbol_get_binary_path (symbol);
              nick = sysprof_symbol_get_binary_nick (symbol);
            }
          else
            {
              name = path = nick = NULL;
            }

          g_string_append_printf (str,
                                  "  %02d: 0x%"G_GINT64_MODIFIER"x:",
                                  j,
                                  sysprof_document_traceable_get_stack_address (traceable, j));

          if (name)
            g_string_append_printf (str, " %s", name);

          if (path)
            g_string_append_printf (str, " [%s]", path);

          if (nick)
            g_string_append_printf (str, " (%s)", nick);

          g_string_append_c (str, '\n');
        }

      g_string_append (str, "  ================\n");

      write (STDOUT_FILENO, str->str, str->len);
    }

  g_print ("Document symbolized\n");

  g_main_loop_quit (main_loop);
}

static gboolean no_bundled;
static const GOptionEntry entries[] = {
  { "no-bundled", 'b', 0, G_OPTION_ARG_NONE, &no_bundled, "Ignore symbols bundled in capture file" },
  { 0 }
};

int
main (int argc,
      char *argv[])
{
  g_autoptr(SysprofDocumentLoader) loader = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;

  main_loop = g_main_loop_new (NULL, FALSE);
  context = g_option_context_new ("- test document symbolization");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  if (argc != 2 || !g_file_test (argv[1], G_FILE_TEST_EXISTS))
    {
      g_printerr ("usage: %s CAPTURE_FILE\n", argv[0]);
      return 1;
    }

  loader = sysprof_document_loader_new (argv[1]);

  if (no_bundled)
    {
      g_autoptr(SysprofMultiSymbolizer) multi = sysprof_multi_symbolizer_new ();

      sysprof_multi_symbolizer_take (multi, sysprof_kallsyms_symbolizer_new ());
      sysprof_multi_symbolizer_take (multi, sysprof_elf_symbolizer_new ());
      sysprof_multi_symbolizer_take (multi, sysprof_jitmap_symbolizer_new ());

      sysprof_document_loader_set_symbolizer (loader, SYSPROF_SYMBOLIZER (multi));
    }

  sysprof_document_loader_load_async (loader, NULL, load_cb, NULL);
  g_main_loop_run (main_loop);

  return 0;
}
