#include "stackstash.h"

typedef struct StackNode StackNode;

struct StackNode
{
    StackNode *	parent;
    gpointer	address;
    StackNode *	siblings;
    StackNode *	children;
    StackNode *	next;		/* next leaf with the same pid */
    int		size;
};

struct StackStash
{
    StackNode  *root;
    GHashTable *leaves_by_process;
};

static StackNode *
stack_node_new (void)
{
    StackNode *node = g_new (StackNode, 1);
    node->siblings = NULL;
    node->children = NULL;
    node->address = NULL;
    node->parent = NULL;
    node->next = NULL;
    node->size = 0;
    return node;
}

static void
stack_node_destroy (gpointer p)
{
    StackNode *node = p;
    if (node)
    {
	stack_node_destroy (node->siblings);
	stack_node_destroy (node->children);
	g_free (node);
    }
}

/* Stach */
StackStash *
stack_stash_new       (void)
{
    StackStash *stash = g_new (StackStash, 1);

    stash->leaves_by_process =
	g_hash_table_new (g_direct_hash, g_direct_equal);
    stash->root = NULL;
    return stash;
}

static StackNode *
stack_node_add_trace (StackNode  *node,
		      GList      *bottom,
		      gint        size,
		      StackNode **leaf)
{
    StackNode *match;
    StackNode *n;

    if (!bottom)
    {
	*leaf = NULL;
	return node;
    }

    if (!bottom->next)
    {
	/* A leaf must always be separate, so pids can
	 * point to them
	 */
	match = NULL;
    }
    else
    {
	for (match = node; match != NULL; match = match->siblings)
	{
	    if (match->address == bottom->data)
		break;
	}
    }

    if (!match)
    {
	match = stack_node_new ();
	match->address = bottom->data;
	match->siblings = node;
	node = match;
    }

    match->children =
	stack_node_add_trace (match->children, bottom->next, size, leaf);

    for (n = match->children; n; n = n->siblings)
	n->parent = match;
    
    if (!bottom->next)
    {
	match->size += size;
	*leaf = match;
    }

    return node;
}

void
stack_stash_add_trace (StackStash *stash,
		       Process    *process,
		       gulong	  *addrs,
		       int         n_addrs,
		       int         size)
{
    GList *trace;
    StackNode *leaf;
    int i;

    trace = NULL;
    for (i = 0; i < n_addrs; ++i)
	trace = g_list_prepend (trace, GINT_TO_POINTER (addrs[i]));

    stash->root = stack_node_add_trace (stash->root, trace, size, &leaf);

    leaf->next = g_hash_table_lookup (
	stash->leaves_by_process, process);
    g_hash_table_insert (
	stash->leaves_by_process, process, leaf);

    g_list_free (trace);
}

typedef struct CallbackInfo
{
    StackFunction func;
    gpointer      data;
} CallbackInfo;

static void
do_callback (gpointer key, gpointer value, gpointer data)
{
    CallbackInfo *info = data;
    Process *process = key;
    StackNode *n;
    StackNode *leaf = value;
    while (leaf)
    {
	GSList *trace;

	trace = NULL;
	for (n = leaf; n; n = n->parent)
	    trace = g_slist_prepend (trace, n->address);

	info->func (process, trace, leaf->size, info->data);

	g_slist_free (trace);
	
	leaf = leaf->next;
    }
}

void
stack_stash_foreach   (StackStash      *stash,
		       StackFunction    stack_func,
		       gpointer         data)
{
    CallbackInfo info;
    info.func = stack_func;
    info.data = data;
	
    g_hash_table_foreach (stash->leaves_by_process, do_callback, &info);
}

static void
stack_node_free (StackNode *node)
{
    if (!node)
	return;

    stack_node_free (node->siblings);
    stack_node_free (node->children);

    g_free (node);
}

void
stack_stash_free	  (StackStash	   *stash)
{
    stack_node_free (stash->root);
    g_hash_table_destroy (stash->leaves_by_process);
}
