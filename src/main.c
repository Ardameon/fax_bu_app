#include "app.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    int res;
    int ret_val = 0;

    app_trace(TRACE_INFO, " << FAX HANDLING APPLICATION BU!!! >>");

    res = app_init();
    if(res)
    {
        app_trace(TRACE_ERR, "app init failed (%d)", res);
        ret_val = -1; goto _exit;
    }

    app_start();

    app_destroy();

_exit:
    return ret_val;
}
