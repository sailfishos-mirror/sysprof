/* MemProf -- memory profiler and leak detector
 * Copyright 1999, 2000, 2001, Red Hat, Inc.
 * Copyright 2002, Kristian Rietveld
 *
 * Sysprof -- Sampling, systemwide CPU profiler
 * Copyright 2004-2005 Soeren Sandmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "process.h"
#include "binfile.h"

#define PAGE_SIZE (getpagesize())

static GHashTable *processes_by_cmdline;
static GHashTable *processes_by_pid;

typedef struct Map Map;
struct Map
{
    char *	filename;
    gulong	start;
    gulong	end;
    gulong      offset;
    gulong	inode;
#if 0
    gboolean	do_offset;
#endif
    
    BinFile *	bin_file;
};

struct Process
{
    char *cmdline;
    
    GList *maps;
    GList *bad_pages;
    
    int pid;

    const char *undefined;
};

static void
initialize (void)
{
    if (!processes_by_cmdline)
    {
	processes_by_cmdline = g_hash_table_new (g_str_hash, g_str_equal);
	processes_by_pid = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
}

static GList *
read_maps (int pid)
{
    char *name = g_strdup_printf ("/proc/%d/maps", pid);
    char buffer[1024];
    FILE *in;
    GList *result = NULL;
    
    in = fopen (name, "r");
    if (!in)
    {
#if 0
	g_print ("could not open %d: %s\n", pid, g_strerror (errno));
#endif
	g_free (name);
	return NULL;
    }
    
    while (fgets (buffer, sizeof (buffer) - 1, in))
    {
	char file[256];
	int count;
	gulong start;
	gulong end;
	gulong offset;
	gulong inode;
	
#if 0
	g_print ("buffer: %s\n", buffer);
#endif
	
	count = sscanf (
	    buffer, "%lx-%lx %*15s %lx %*x:%*x %lu %255s", 
	    &start, &end, &offset, &inode, file);
	if (count == 5)
	{
	    Map *map;
	    
	    map = g_new (Map, 1);
	    
	    map->filename = g_strdup (file);
	    map->start = start;
	    map->end = end;
	    
	    map->offset = offset;

	    map->inode = inode;
	    
	    map->bin_file = NULL;
	    
	    result = g_list_prepend (result, map);
	}
#if 0
	else
	{
	    g_print ("scanf\n");
	}
#endif
    }
    
    g_free (name);
    fclose (in);
    return result;
}


static Process *
create_process (const char *cmdline, int pid)
{
    Process *p;
    
    p = g_new0 (Process, 1);
    
    if (*cmdline != '\0')
	p->cmdline = g_strdup_printf ("[%s]", cmdline);
    else
	p->cmdline = g_strdup_printf ("[pid %d]", pid);
    
    p->bad_pages = NULL;
    p->maps = NULL;
    p->pid = pid;
    p->undefined = NULL;
    
    g_assert (!g_hash_table_lookup (processes_by_pid, GINT_TO_POINTER (pid)));
    g_assert (!g_hash_table_lookup (processes_by_cmdline, cmdline));
    
    g_hash_table_insert (processes_by_pid, GINT_TO_POINTER (pid), p);
    g_hash_table_insert (processes_by_cmdline, g_strdup (cmdline), p);
    
    return p;
}

static Map *
process_locate_map (Process *process, gulong addr)
{
    GList *list;
    
    for (list = process->maps; list != NULL; list = list->next)
    {
	Map *map = list->data;
	
	if ((addr >= map->start) &&
	    (addr < map->end))
	{
	    return map;
	}
    }
    
    return NULL;
}

static void
process_free_maps (Process *process)
{
    GList *list;
    
    for (list = process->maps; list != NULL; list = list->next)
    {
	Map *map = list->data;
	
	if (map->filename)
	    g_free (map->filename);
	
	if (map->bin_file)
	    bin_file_free (map->bin_file);
	
	g_free (map);
    }
    
    g_list_free (process->maps);
}


static void
free_process (gpointer key, gpointer value, gpointer data)
{
    char *cmdline = key;
    Process *process = value;
    
#if 0
    g_print ("freeing: %p\n", process);
    memset (process, '\0', sizeof (Process));
#endif
    g_free (process->cmdline);
#if 0
    process->cmdline = "You are using free()'d memory";
#endif
    process_free_maps (process);
    g_list_free (process->bad_pages);
    g_free (cmdline);
    
    g_free (process);
}

void
process_flush_caches  (void)
{
    if (!processes_by_cmdline)
	return;

    g_hash_table_foreach (processes_by_cmdline, free_process, NULL);
    
    g_hash_table_destroy (processes_by_cmdline);
    g_hash_table_destroy (processes_by_pid);
    
    processes_by_cmdline = NULL;
    processes_by_pid = NULL;
}


static gboolean
process_has_page (Process *process, gulong addr)
{
    if (process_locate_map (process, addr))
	return TRUE;
    else
	return FALSE;
}

void
process_ensure_map (Process *process, int pid, gulong addr)
{
    /* Round down to closest page */
    
    addr = (addr - addr % PAGE_SIZE);
    
    if (process_has_page (process, addr))
	return;
    
    if (g_list_find (process->bad_pages, (gpointer)addr))
	return;
    
    /* a map containing addr was not found */
    if (process->maps)
	process_free_maps (process);
    
    process->maps = read_maps (pid);
    
    if (!process_has_page (process, addr))
    {
#if 0
	g_print ("Bad page: %p\n", addr);
#endif
	process->bad_pages = g_list_prepend (process->bad_pages, (gpointer)addr);
    }
}

static gboolean
do_idle_free (gpointer d)
{
    g_free (d);
    return FALSE;
}

static char *
idle_free (char *d)
{
    g_idle_add (do_idle_free, d);
    return d;
}

static char *
get_cmdline (int pid)
{
    char *cmdline;
    char *filename = idle_free (g_strdup_printf ("/proc/%d/cmdline", pid));
    
    if (g_file_get_contents (filename, &cmdline, NULL, NULL))
    {
	if (*cmdline == '\0')
	{
	    g_free (cmdline);
	    return NULL;
 	}
	return cmdline;
    }
    
    return NULL;
}

static char *
get_statname (int pid)
{
    char *stat;
    char *filename = idle_free (g_strdup_printf ("/proc/%d/stat", pid));
    
#if 0
    g_print ("stat %d\n", pid);
#endif
    
    if (g_file_get_contents (filename, &stat, NULL, NULL))
    {
	char result[200];
	
	idle_free (stat);
	
	if (sscanf (stat, "%*d %200s %*s", result) == 1)
	    return g_strndup (result, 200);
    }
#if 0
    g_print ("return null\n");
#endif
    
    return NULL;
}

static char *
get_pidname (int pid)
{
    if (pid == -1)
	return g_strdup_printf ("kernel");
    else
	return g_strdup_printf ("pid %d", pid);
}

static char *
get_name (int pid)
{
    char *cmdline = NULL;
    
    if ((cmdline = get_cmdline (pid)))
	return cmdline;
    
    if ((cmdline = get_statname (pid)))
	return cmdline;
    
    return get_pidname (pid);
}

Process *
process_get_from_pid (int pid)
{
    Process *p;
    gchar *cmdline = NULL;
    
    initialize();
    
    p = g_hash_table_lookup (processes_by_pid, GINT_TO_POINTER (pid));
    
    if (p)
	return p;
    
    cmdline = get_name (pid);
    
    idle_free (cmdline);
    
    p = g_hash_table_lookup (processes_by_cmdline, cmdline);
    if (!p)
	p = create_process (cmdline, pid);
    
    return p;
}



#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

static gboolean
file_exists (const char *name)
{
    int fd;
    fd = open (name, O_RDONLY);
    
    g_print ("trying: %s\n", name);
    
    if (fd > 0)
    {
	close (fd);
	return TRUE;
    }
    return FALSE;
}

static gchar *
look_for_vmlinux (void)
{
    struct utsname utsname;
    char *result;
    GList *names;
    GList *list;
    uname (&utsname);
    
    names = NULL;
    
    names = g_list_prepend (
	names, g_strdup_printf (
	    "/usr/lib/debug/lib/modules/%s/vmlinux", utsname.release));
    
    names = g_list_prepend (
	names, g_strdup_printf (
	    "/boot/vmlinux-%s", utsname.release));
    
    result = NULL;
    
    for (list = names; list != NULL; list = list->next)
    {
	char *name = list->data;
	
	if (file_exists (name))
	{
	    result = g_strdup (name);
	    break;
	}
    }
    
    g_list_foreach (names, (GFunc)g_free, NULL);
    g_list_free (names);
    
    return result;
}

static const gchar *
find_kernel_binary (void)
{
    static gboolean looked_for_vmlinux;
    static gchar *binary = NULL;
    
    if (!looked_for_vmlinux)
    {
	binary = look_for_vmlinux ();
	looked_for_vmlinux = TRUE;
    }
    
    return binary;
}

static void
parse_kallsym_line (const char *line,
		    GHashTable *table)
{
    char **tokens = g_strsplit_set (line, " \t", -1);

    if (tokens[0] && tokens[1] && tokens[2])
    {
	glong address;
	char *endptr;

	address = strtoul (tokens[0], &endptr, 16);

	if (*endptr == '\0')
	{
	    g_hash_table_insert (
		table, GUINT_TO_POINTER (address), g_strdup (tokens[2]));
	}
    }

    g_strfreev (tokens);
}

static void
parse_kallsyms (const char *kallsyms,
		GHashTable *table)
{
    const char *sol;
    const char *eol;
    
    sol = kallsyms;
    eol = strchr (sol, '\n');
    while (eol)
    {
	char *line = g_strndup (sol, eol - sol);
	
	parse_kallsym_line (line, table);
	
	g_free (line);
	
	sol = eol + 1;
	eol = strchr (sol, '\n');
    }
}

static GHashTable *
get_kernel_symbols (void)
{
    static GHashTable *kernel_syms;
    
    if (!kernel_syms)
    {
	char *kallsyms;
	g_file_get_contents ("/proc/kallsyms", &kallsyms, NULL, NULL);
	
	if (kallsyms)
	{
	    kernel_syms = g_hash_table_new_full (g_direct_hash, g_direct_equal,
						 NULL, g_free);

	    parse_kallsyms (kallsyms, kernel_syms);
	    
	    g_free (kallsyms);
	    g_hash_table_destroy (kernel_syms);
	}
    }

    return NULL;
}

static const char *
lookup_kernel_symbol (gulong address)
{
    static const char *const kernel = "In kernel";

#if 0
    g_print ("kernel binary: %s\n", find_kernel_binary ());
#endif

    return kernel; /* Can we just return "In kernel"? */
#if 0
    kernel.name = "In kernel";
    kernel.address = 0x00001337;
    return &kernel;
#endif
}

const char *
process_lookup_symbol (Process *process, gulong address)
{
    const BinSymbol *result;
    Map *map = process_locate_map (process, address);
    
/*     g_print ("addr: %x\n", address); */

    if (address == 0x1)
    {
	get_kernel_symbols ();
	
	return lookup_kernel_symbol (address);
    }
    else if (!map)
    {
	if (!process->undefined)
	{
	    process->undefined =
		g_strdup_printf ("No map (%s)", process->cmdline);
	}
	
	return process->undefined;
    }

#if 0
    if (strcmp (map->filename, "/home/ssp/sysprof/sysprof") == 0)
    {
	g_print ("YES\n");

	g_print ("map address: %lx\n", map->start);
	g_print ("map offset: %lx\n", map->offset);
	g_print ("address before: %lx  (%s)\n", address, map->filename);
    }

    g_print ("address before: \n");
#endif
    
    address -= map->start;
    address += map->offset;
    
#if 0
    address -= map->start;
    address += map->offset;
#endif

#if 0
    if (strcmp (map->filename, "/home/ssp/sysprof/sysprof") == 0)
    {
	g_print ("address after: %lx\n", address);
    }
#endif
    
    if (!map->bin_file)
	map->bin_file = bin_file_new (map->filename);

/*     g_print ("%s: start: %p, load: %p\n", */
/* 	     map->filename, map->start, bin_file_get_load_address (map->bin_file)); */

    if (map->inode != bin_file_get_inode (map->bin_file))
    {
	/* If the inodes don't match, it's probably because the
	 * file has changed since the process started. Just return
	 * the undefined symbol in that case.
	 */
	
	address = 0x0;
    }

    result = bin_file_lookup_symbol (map->bin_file, address);
    
#if 0
    g_print (" ---> %s\n", result->name);
#endif
    
/*     g_print ("(%x) %x %x name; %s\n", address, map->start, map->offset, result->name); */
    
    return bin_symbol_get_name (map->bin_file, result);
}

const char *
process_get_cmdline (Process *process)
{
    return process->cmdline;
}
