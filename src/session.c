#include "session.h"


static int session_id_array[SESSION_ID_CNT];

static int ses_id_static_in = 0;
static int ses_id_static_out = SESSION_ID_OUT;


/*============================================================================*/


int session_NextID(int *ses_id, int in)
{
    int i = 0;
    int ret_val = -1;

    *ses_id = -1;

    if(in)
    {
        for(i = ses_id_static_in; i < SESSION_ID_OUT; i++)
        {
            if(session_id_array[i] == 0)
            {
                *ses_id = i;
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = 0;
                goto _exit;
            }
        }

        for(i = SESSION_ID_IN; i < ses_id_static_in; i++)
        {
            if(session_id_array[i] == 0)
            {
                *ses_id = i;
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = 0;
                goto _exit;
            }
        }
    } else {
        for(i = ses_id_static_out; i < SESSION_ID_CNT; i++)
        {
            if(session_id_array[i] == 0)
            {
                *ses_id = i;
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = 0;
                goto _exit;
            }
        }

        for(i = SESSION_ID_OUT; i < ses_id_static_out; i++)
        {
            if(session_id_array[i] == 0)
            {
                *ses_id = i;
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = 0;
                goto _exit;
            }
        }
    }

_exit:
    return ret_val;
}


