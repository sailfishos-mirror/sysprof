/* sysprof-document-symbols.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "sysprof-document-private.h"
#include "sysprof-document-symbols-private.h"
#include "sysprof-document-traceable.h"
#include "sysprof-symbol-private.h"
#include "sysprof-symbolizer-private.h"

struct _SysprofDocumentSymbols
{
  GObject parent_instance;

  SysprofSymbol *context_switches[SYSPROF_ADDRESS_CONTEXT_GUEST_USER+1];
};

G_DEFINE_FINAL_TYPE (SysprofDocumentSymbols, sysprof_document_symbols, G_TYPE_OBJECT)

static void
sysprof_document_symbols_finalize (GObject *object)
{
  SysprofDocumentSymbols *self = (SysprofDocumentSymbols *)object;

  for (guint i = 0; i < G_N_ELEMENTS (self->context_switches); i++)
    g_clear_object (&self->context_switches[i]);

  G_OBJECT_CLASS (sysprof_document_symbols_parent_class)->finalize (object);
}

static void
sysprof_document_symbols_class_init (SysprofDocumentSymbolsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = sysprof_document_symbols_finalize;
}

static void
sysprof_document_symbols_init (SysprofDocumentSymbols *self)
{
}

typedef struct _Symbolize
{
  SysprofDocument        *document;
  SysprofSymbolizer      *symbolizer;
  SysprofDocumentSymbols *symbols;
} Symbolize;

static void
symbolize_free (Symbolize *state)
{
  g_clear_object (&state->document);
  g_clear_object (&state->symbolizer);
  g_clear_object (&state->symbols);
  g_free (state);
}

static void
sysprof_document_symbols_add_traceable (SysprofDocumentSymbols   *self,
                                        SysprofDocumentTraceable *traceable,
                                        SysprofSymbolizer        *symbolizer)
{
  SysprofAddressContext last_context;
  guint64 *addresses;
  guint n_addresses;

  g_assert (SYSPROF_IS_DOCUMENT_SYMBOLS (self));
  g_assert (SYSPROF_IS_DOCUMENT_TRACEABLE (traceable));
  g_assert (SYSPROF_IS_SYMBOLIZER (symbolizer));

  n_addresses = sysprof_document_traceable_get_stack_depth (traceable);
  addresses = g_alloca (sizeof (guint64) * n_addresses);
  sysprof_document_traceable_get_stack_addresses (traceable, addresses, n_addresses);

  last_context = SYSPROF_ADDRESS_CONTEXT_NONE;

  for (guint i = 0; i < n_addresses; i++)
    {
      SysprofAddress address = addresses[i];
      SysprofAddressContext context = SYSPROF_ADDRESS_CONTEXT_NONE;

      if (sysprof_address_is_context_switch (address, &context))
        {
          last_context = context;
        }
      else
        {
        }
    }
}

static void
sysprof_document_symbols_worker (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  static const struct {
    const char *name;
    guint value;
  } context_switches[] = {
    { "- - Hypervisor - -", SYSPROF_ADDRESS_CONTEXT_HYPERVISOR },
    { "- - Kernel - -", SYSPROF_ADDRESS_CONTEXT_KERNEL },
    { "- - User - -", SYSPROF_ADDRESS_CONTEXT_USER },
    { "- - Guest - -", SYSPROF_ADDRESS_CONTEXT_GUEST },
    { "- - Guest Kernel - -", SYSPROF_ADDRESS_CONTEXT_GUEST_KERNEL },
    { "- - Guest User - -", SYSPROF_ADDRESS_CONTEXT_GUEST_USER },
  };
  Symbolize *state = task_data;
  GtkBitsetIter iter;
  GtkBitset *bitset;
  GListModel *model;
  guint i;

  g_assert (source_object == NULL);
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (state != NULL);
  g_assert (SYSPROF_IS_DOCUMENT (state->document));
  g_assert (SYSPROF_IS_SYMBOLIZER (state->symbolizer));
  g_assert (SYSPROF_IS_DOCUMENT_SYMBOLS (state->symbols));

  bitset = _sysprof_document_traceables (state->document);
  model = G_LIST_MODEL (state->document);

  /* Create static symbols for context switch use */
  for (guint cs = 0; cs < G_N_ELEMENTS (context_switches); cs++)
    {
      g_autoptr(GRefString) name = g_ref_string_new_intern (context_switches[cs].name);
      g_autoptr(SysprofSymbol) symbol = _sysprof_symbol_new (name, NULL, NULL);

      /* TODO: It would be nice if we had enough insight from the capture header
       * as to the host system, so we can show "vmlinuz" and "Linux" respectively
       * for binary-path and binary-nick when the capture came from Linux.
       */

      state->symbols->context_switches[context_switches[cs].value] = g_steal_pointer (&symbol);
    }

  /* Walk through the available traceables which need symbols extracted */
  if (gtk_bitset_iter_init_first (&iter, bitset, &i))
    {
      do
        {
          g_autoptr(SysprofDocumentTraceable) traceable = g_list_model_get_item (model, i);

          if (SYSPROF_IS_DOCUMENT_TRACEABLE (traceable))
            sysprof_document_symbols_add_traceable (state->symbols,
                                                    traceable,
                                                    state->symbolizer);
        }
      while (gtk_bitset_iter_next (&iter, &i));
    }

  g_task_return_pointer (task,
                         g_object_ref (state->symbols),
                         g_object_unref);
}

void
_sysprof_document_symbols_new (SysprofDocument     *document,
                               SysprofSymbolizer   *symbolizer,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  Symbolize *state;

  g_return_if_fail (SYSPROF_IS_DOCUMENT (document));
  g_return_if_fail (SYSPROF_IS_SYMBOLIZER (symbolizer));

  state = g_new0 (Symbolize, 1);
  state->document = g_object_ref (document);
  state->symbolizer = g_object_ref (symbolizer);
  state->symbols = g_object_new (SYSPROF_TYPE_DOCUMENT_SYMBOLS, NULL);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, _sysprof_document_symbols_new);
  g_task_set_task_data (task, state, (GDestroyNotify)symbolize_free);
  g_task_run_in_thread (task, sysprof_document_symbols_worker);
}

SysprofDocumentSymbols *
_sysprof_document_symbols_new_finish (GAsyncResult  *result,
                                      GError       **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == _sysprof_document_symbols_new, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * sysprof_document_symbols_lookup:
 * @self: a #SysprofDocumentSymbols
 * @pid: the process identifier
 * @context: the #SysprofAddressContext for the address
 * @address: a #SysprofAddress to lookup the symbol for
 *
 * Locates the symbol that is found at @address within @context of @pid.
 *
 * Returns: (transfer none) (nullable): a #SysprofSymbol or %NULL
 */
SysprofSymbol *
sysprof_document_symbols_lookup (SysprofDocumentSymbols *self,
                                 int                     pid,
                                 SysprofAddressContext   context,
                                 SysprofAddress          address)
{
  SysprofAddressContext new_context;

  g_return_val_if_fail (SYSPROF_IS_DOCUMENT_SYMBOLS (self), NULL);
  g_return_val_if_fail (context <= SYSPROF_ADDRESS_CONTEXT_GUEST_USER, NULL);

  if (sysprof_address_is_context_switch (address, &new_context))
    return self->context_switches[new_context];

  return NULL;
}
