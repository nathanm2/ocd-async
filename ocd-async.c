
#include <gio/gio.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>

char *server = NULL;
int port = 6666;

static GOptionEntry entries[] = {
    {"server", 's', 0, G_OPTION_ARG_STRING, &server, "OpenOCD server",
     "<HOST>{:<PORT>}"},
    {"port", 'p', 0, G_OPTION_ARG_INT, &port, "OpenOCD Server port", "<PORT>"},
    {0},
};

typedef struct {
    char *cmd;
    GMainLoop *loop;
} Context;

static void show_help(GOptionContext *opt_ctx)
{
    char *help_str = g_option_context_get_help(opt_ctx, false, NULL);
    fwrite(help_str, 1, strlen(help_str), stdout);
    g_free(help_str);
}

static void connected(GObject *source, GAsyncResult *res, gpointer user_data)
{
    printf("Connected!\n");
}

int main(int argc, char *argv[])
{
    GOptionContext *oc;
    GError *error = NULL;
    GSocketClient *sockc;
    Context ctx = {0};

    setlocale(LC_ALL, "");

    /* Process CLI arguments: */
    oc = g_option_context_new("<openocd_cmd> [<arg> ...]");
    g_option_context_add_main_entries(oc, entries, NULL);
    g_option_context_set_strict_posix(oc, true);
    if (!g_option_context_parse(oc, &argc, &argv, &error)) {
        g_printerr("%s\n", error->message);
        exit(1);
    }

    if (argc < 2) {
        show_help(oc);
        exit(1);
    }
    g_option_context_free(oc);

    /* Construct OpenOCD command:
     *
     * NOTE: Command string must be terminated with 0x1A
     */
    GString *tmp = g_string_new(argv[1]);
    for (int i = 2; i < argc; i++)
        g_string_append_printf(tmp, " %s", argv[i]);
    g_string_append_c(tmp, 0x1a);
    ctx.cmd = g_string_free(tmp, false);

    ctx.loop = g_main_loop_new(NULL, false);

    /* Connect to OpenOCD server (async) */
    sockc = g_socket_client_new();
    g_socket_client_connect_to_host_async(sockc, server ?: "localhost", port,
                                          NULL, connected, &ctx);

    /* Let's do this! */
    g_main_loop_run(ctx.loop);

    return 0;
}
