
#include <gio/gio.h>
#include <locale.h>
#include <stdio.h>

gint port = 6666;

static GOptionEntry entries[] = {
    {"port", 'p', 0, G_OPTION_ARG_INT, &port, "OpenOCD TCL Port."},
    {0},
};

int main(int argc, char *argv[])
{
    GOptionContext *oc;
    GError *error = NULL;
    GSocketClient *sockc;
    GSocketConnection *con;
    GInputStream *istream;
    GOutputStream *ostream;

    setlocale(LC_CTYPE, "");

    /* Process CLI arguments: */
    oc = g_option_context_new(NULL);
    g_option_context_add_main_entries(oc, entries, NULL);
    if (!g_option_context_parse(oc, &argc, &argv, &error)) {
        g_printerr("%s\n", error->message);
        exit(1);
    }
    g_option_context_free(oc);

    /* Open a TCP connection to the specified port: */
    sockc = g_socket_client_new();
    con =
        g_socket_client_connect_to_host(sockc, "localhost", port, NULL, &error);

    istream = g_io_stream_get_input_stream(G_IO_STREAM(con));
    ostream = g_io_stream_get_output_stream(G_IO_STREAM(con));

    const char *msg = "delayed_response 42\x1a";
    char response[4096];
    g_output_stream_write(ostream, msg, strlen(msg), NULL, NULL);
    g_input_stream_read(istream, response, sizeof(response), NULL, NULL);

    printf("%s\n", response);
    return 0;
}
