#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE

#include <polkitagent/polkitagent.h>
#include <polkit/polkit.h>
#include <gio/gio.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/* =========================================================================
 * Utility: read process start time from /proc/self/stat
 * ========================================================================= */

static gboolean read_proc_start_time(guint64 *out_start_time)
{
    if (!out_start_time) return FALSE;

    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return FALSE;

    /*
     * Format:
     * pid (comm) state ppid pgrp session tty_nr tpgid flags
     * minflt cminflt majflt cmajflt utime stime cutime cstime
     * priority nice num_threads itrealvalue starttime ...
     */

    int c;
    long pid;
    char state;

    if (fscanf(f, "%ld", &pid) != 1) goto fail;

    /* skip " (comm) " */
    c = fgetc(f); /* space */
    c = fgetc(f); /* '(' */
    while ((c = fgetc(f)) != EOF && c != ')');
    c = fgetc(f); /* space */

    if (fscanf(f, " %c", &state) != 1) goto fail;

    /* skip fields 4–21 */
    unsigned long long dummy;
    for (int i = 0; i < 18; ++i)
        if (fscanf(f, " %llu", &dummy) != 1) goto fail;

    /* field 22: starttime */
    if (fscanf(f, " %llu", out_start_time) != 1) goto fail;

    fclose(f);
    return TRUE;

fail:
    fclose(f);
    return FALSE;
}

/* =========================================================================
 * WospAgent GObject
 * ========================================================================= */

typedef struct _WospAgent {
    PolkitAgentListener parent;
} WospAgent;

typedef struct _WospAgentClass {
    PolkitAgentListenerClass parent_class;
} WospAgentClass;

G_DEFINE_TYPE(WospAgent, wosp_agent, POLKIT_AGENT_TYPE_LISTENER)

/* -------------------------------------------------------------------------
 * initiate_authentication (Debian ABI)
 * ------------------------------------------------------------------------- */

static void
wosp_agent_initiate_authentication(
    PolkitAgentListener *listener,
    const gchar         *action_id,
    const gchar         *message,
    const gchar         *icon_name,
    PolkitDetails       *details,
    const gchar         *cookie,
    GList               *identities,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data)
{
    (void)listener;
    (void)action_id;
    (void)message;
    (void)icon_name;
    (void)details;
    (void)cookie;
    (void)identities;

    int rc = system("/usr/local/bin/wosp-lock --auth");

    GTask *task = g_task_new(NULL, cancellable, callback, user_data);
    g_task_return_boolean(task, rc == 0);
    g_object_unref(task);
}

static gboolean
wosp_agent_initiate_authentication_finish(
    PolkitAgentListener *listener,
    GAsyncResult        *res,
    GError             **error)
{
    (void)listener;
    return g_task_propagate_boolean(G_TASK(res), error);
}

/* -------------------------------------------------------------------------
 * class init
 * ------------------------------------------------------------------------- */

static void
wosp_agent_class_init(WospAgentClass *klass)
{
    PolkitAgentListenerClass *lc = POLKIT_AGENT_LISTENER_CLASS(klass);
    lc->initiate_authentication = wosp_agent_initiate_authentication;
    lc->initiate_authentication_finish = wosp_agent_initiate_authentication_finish;
}

static void
wosp_agent_init(WospAgent *agent)
{
    (void)agent;
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    WospAgent *agent =
        (WospAgent *)g_object_new(wosp_agent_get_type(), NULL);

    guint64 start_time = 0;
    if (!read_proc_start_time(&start_time)) {
        g_printerr("Failed to read process start time\n");
        return 1;
    }

    PolkitSubject *subject =
        polkit_unix_process_new_for_owner(
            getpid(),
            start_time,
            getuid()
        );

    GError *error = NULL;

    polkit_agent_listener_register(
        POLKIT_AGENT_LISTENER(agent),
        POLKIT_AGENT_REGISTER_FLAGS_NONE,
        subject,
        "/org/wosp/PolkitAgent",
        NULL,
        &error
    );

    if (error) {
        g_printerr("Failed to register polkit agent: %s\n", error->message);
        return 1;
    }

    g_main_loop_run(loop);
    return 0;
}
