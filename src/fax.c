/*
 *  To get more understanding what is FAX, T30, T37, T38 and how FoIP works,
 *  read the:
 *  "Fax, Modem, and Text for IP Telephony"(David Hanes, Gonzalo Salgueiro,2008)
 *  and also ITU official documentation (T30, T38, T37).
 *
 */
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
//#include <spandsp/t4_rx.h>
#include <sys/poll.h>
#include <pthread.h>

#include "fax.h"

#define THIS_FILE "fax.c"

#define DEF_T38_MAX_BITRATE       9600
#define DEF_T38_FAX_VERSION       0
#define DEF_T38_FILL_BIT_REMOVAL  0
#define DEF_T38_TRANSCODING_MMR   0
#define DEF_T38_TRANSCODING_JBIG  0
#define DEF_T38_RATE_MANAGEMNT    "transferredTCF"
#define DEF_T38_MAX_BUFFER        72
#define DEF_T38_MAX_DATAGRAM      316
#define DEF_T38_VENDOR_INFO       "Tyryshkin M V"
#define DEF_T38_UDP_EC            "t38UDPRedundancy"

#define DEF_FAX_IDENT             "FAX_TRANSMITTER"
#define DEF_FAX_HEADER            "FAX_DEFAULT_HEADER"
#define DEF_FAX_VERBOSE           0
#define DEF_FAX_USE_ECM           1
#define DEF_FAX_DISABLE_V17       0

#define MAX_FEC_ENTRIES           4
#define MAX_FEC_SPAN              4
#define DEFAULT_FEC_ENTRIES       3
#define DEFAULT_FEC_SPAN          3

#define FRAMES_PER_CHUNK          160

#define MAX_MSG_SIZE 1500

#define TRANSMIT_ON_IDLE          1
#define TEP_MODE                  0

static void fax_paramsInit(fax_params_t *f_params);
static void fax_paramsDestroy(fax_params_t *f_params);
static void fax_paramsSetDefault(fax_params_t *f_params);

/*============================================================================*/

static void fax_spandspLog(int level, const char *msg)
{
	switch (level) {
		case SPAN_LOG_NONE:
			return;
		case SPAN_LOG_ERROR:
		case SPAN_LOG_PROTOCOL_ERROR:
			if(msg) app_trace(TRACE_INFO, "SPANDSP_LOG %s", msg);
			break;
		case SPAN_LOG_WARNING:
		case SPAN_LOG_PROTOCOL_WARNING:
			if(msg) app_trace(TRACE_INFO, "SPANDSP_LOG %s", msg);
			break;
		case SPAN_LOG_FLOW:
		case SPAN_LOG_FLOW_2:
		case SPAN_LOG_FLOW_3:
		default:	/* SPAN_LOG_DEBUG, SPAN_LOG_DEBUG_2, SPAN_LOG_DEBUG_3 */
			if(msg) app_trace(TRACE_INFO, "SPANDSP_LOG %s", msg);
			break;
	}
}

/*============================================================================*/

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data,
								 const uint8_t *buf, int len, int count)
{
	fax_params_t *f_params;
	session_t *session;
	uint8_t pkt[LOCAL_FAX_MAX_DATAGRAM];
	int udptl_packtlen;
	int x;
	int ret_val = 0;
	int res = 0;

	(void)s;

	f_params = (fax_params_t *)user_data;
	session = f_params->session;

	if((udptl_packtlen = udptl_build_packet(f_params->pvt.udptl_state,
											pkt, buf, len)) > 0)
	{
		for(x = 0; x < count; x++)
		{
			res = f_params->send_cb(session, pkt, udptl_packtlen);

			if(res < 0)
			{
				app_trace(TRACE_ERR,"Fax %04x. send() failed (%d) %s",
						  session->ses_id, res,
						  res == -1 ? strerror(errno) : "");
				ret_val = -1;
				break;
			}
		}
	} else {
		app_trace(TRACE_ERR, "Fax %04x. Invalid UDPTL packet len: %d"
				  " PASSED: %d:%d", session->ses_id, udptl_packtlen,
				  len, count);
	}

	return ret_val;
}

/*============================================================================*/

static int fax_initGW(fax_params_t *f_params)
{
    t38_gateway_state_t *t38_gw;
    t38_core_state_t *t38_core;
    logging_state_t *logging;
    session_t *session;
    int log_level;
    int ret_val = 0;
    int fec_entries = DEFAULT_FEC_ENTRIES;
    int fec_span = DEFAULT_FEC_SPAN;
    int supported_modems;

    if(!f_params)
    {
        ret_val = -1; goto _exit;
    }

    session = f_params->session;

    app_trace(TRACE_INFO, "Fax %04x. Init T.38-fax gateway", session->ses_id);

    memset(f_params->pvt.t38_gw_state, 0, sizeof(t38_gateway_state_t));

    if(t38_gateway_init(f_params->pvt.t38_gw_state, t38_tx_packet_handler,
                         f_params) == NULL)
    {
        app_trace(TRACE_ERR, "Fax %04x. Cannot initialize T.38 structs",
                  session->ses_id);
        ret_val = -2; goto _exit;
    }

    t38_gw = f_params->pvt.t38_gw_state;

    t38_core = f_params->pvt.t38_core = t38_gateway_get_t38_core_state(t38_gw);

    t38_gateway_set_transmit_on_idle(t38_gw, TRANSMIT_ON_IDLE);
    t38_gateway_set_tep_mode(t38_gw, TEP_MODE);

    if(udptl_init(f_params->pvt.udptl_state, UDPTL_ERROR_CORRECTION_REDUNDANCY,
                  fec_span, fec_entries,
                  (udptl_rx_packet_handler_t *) t38_core_rx_ifp_packet,
                  (void *) f_params->pvt.t38_core) == NULL)
    {
        app_trace(TRACE_ERR, "Fax %04x. Cannot initialize UDPTL structs",
                  session->ses_id);
        ret_val = -3; goto _exit;
    }

    if(f_params->pvt.verbose)
    {
        log_level = SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG |
                SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL |
                SPAN_LOG_FLOW;

        logging = t38_gateway_get_logging_state(t38_gw);
        span_log_set_message_handler(logging, fax_spandspLog);
        span_log_set_level(logging, log_level);
        span_log_set_tag(logging, f_params->log_tag);

        logging = t38_core_get_logging_state(t38_core);
        span_log_set_message_handler(logging, fax_spandspLog);
        span_log_set_level(logging, log_level);
        span_log_set_tag(logging, f_params->log_tag);
    }

	supported_modems = T30_SUPPORT_V29 | T30_SUPPORT_V27TER;
	if(!f_params->pvt.disable_v17) supported_modems |= T30_SUPPORT_V17;

	t38_gateway_set_supported_modems(t38_gw, supported_modems);
	t38_gateway_set_ecm_capability(t38_gw, f_params->pvt.use_ecm);

_exit:
	return ret_val;
}

/*============================================================================*/

static int fax_releaseGW(fax_params_t *f_params)
{
	session_t *session = f_params->session;

	app_trace(TRACE_INFO, "Fax %04x. Release T.38-fax gateway", session->ses_id);

	if(f_params->pvt.t38_gw_state)
	{
		t38_gateway_release(f_params->pvt.t38_gw_state);
	}

	if(f_params->pvt.udptl_state)
	{
		udptl_release(f_params->pvt.udptl_state);
	}

	return 0;
}

/*============================================================================*/

static int configure_t38(fax_params_t *f_params)
{
	int method = 2;

	t38_set_t38_version(f_params->pvt.t38_core,
						f_params->t38_options.T38FaxVersion);
	t38_set_max_buffer_size(f_params->pvt.t38_core,
							f_params->t38_options.T38FaxMaxBuffer);
	t38_set_fastest_image_data_rate(f_params->pvt.t38_core,
									f_params->t38_options.T38MaxBitRate);
	t38_set_fill_bit_removal(f_params->pvt.t38_core,
							 f_params->t38_options.T38FaxFillBitRemoval);
	t38_set_mmr_transcoding(f_params->pvt.t38_core,
							f_params->t38_options.T38FaxTranscodingMMR);
	t38_set_jbig_transcoding(f_params->pvt.t38_core,
							 f_params->t38_options.T38FaxTranscodingJBIG);
	t38_set_max_datagram_size(f_params->pvt.t38_core,
							  f_params->t38_options.T38FaxMaxDatagram);

	if(f_params->t38_options.T38FaxRateManagement)
	{
		if(!strcasecmp(f_params->t38_options.T38FaxRateManagement,
					   "transferredTCF"))
		{
			method = 1;
		} else {
			method = 2;
		}
	}

	t38_set_data_rate_management_method(f_params->pvt.t38_core, method);

	return 0;
}

/*============================================================================*/

int fax_rxUDPTL(const session_t *session, const uint8_t *buf, int len)
{
    int ret_val = 0;
    int res = 0;

    if(!session || !buf)
    {
        ret_val = -1; goto _exit;
    }

    res = udptl_rx_packet(session->fax_params.pvt.udptl_state,
                          buf, len);
    if(res)
    {
        app_trace(TRACE_ERR, "Fax %04x. UDPTL RX failed (%d)",
                  session->ses_id, res);
        ret_val = -2;
    }

_exit:
    return ret_val;
}

/*============================================================================*/

int fax_rxAUDIO(const session_t *session, const uint8_t *buf, int len)
{
    int ret_val = 0;
    int res = 0;

    if(!session || !buf)
    {
        ret_val = -1; goto _exit;
    }

    res = t38_gateway_rx(session->fax_params.pvt.t38_gw_state,
                         (int16_t *)buf, len / 2);
    if(res)
    {
        app_trace(TRACE_ERR, "Fax %04x. AUDIO RX failed (%d)",
                  session->ses_id, res);
        ret_val = -2;
    }

_exit:
    return ret_val;
}

/*============================================================================*/

int fax_txAUDIO(const session_t *session, const uint8_t *buf, int *len)
{
    int ret_val = 0;
    int res = 0;

    if(!session || !buf)
    {
        ret_val = -1; goto _exit;
    }

    res = t38_gateway_tx(session->fax_params.pvt.t38_gw_state,
                         (int16_t *)buf, FRAMES_PER_CHUNK);
    if(res < 0)
    {
        app_trace(TRACE_ERR, "Fax %04x. AUDIO TX failed (%d)",
                  session->ses_id, res);
        ret_val = -2;
    }

    *len = res * 2;

_exit:
    return ret_val;
}

/*============================================================================*/

int fax_sessionInit(session_t *session, t38_send_callback *send_cb)
{
    int ret_val = 0;
    fax_params_t *fax_params = &session->fax_params;

    app_trace(TRACE_INFO, "Session %04x. Init FAX", session->ses_id);

    fax_params->session = session;
    fax_params->send_cb = send_cb;

    fax_paramsInit(fax_params);
    fax_paramsSetDefault(fax_params);

    ret_val = fax_initGW(fax_params);

    if(!ret_val) configure_t38(fax_params);

    return ret_val;
}

/*============================================================================*/

int fax_sessionDestroy(session_t *session)
{
    app_trace(TRACE_INFO, "Session %04x. Destroy FAX", session->ses_id);

    fax_releaseGW(&session->fax_params);
    fax_paramsDestroy(&session->fax_params);

    return 0;
}

/*============================================================================*/

static void fax_paramsInit(fax_params_t *f_params)
{
    f_params->pvt.t38_gw_state = malloc(sizeof(t38_gateway_state_t));
    f_params->pvt.udptl_state = malloc(sizeof(udptl_state_t));

    f_params->pvt.header = NULL;
    f_params->pvt.ident = NULL;

    f_params->t38_options.T38FaxUdpEC = NULL;
    f_params->t38_options.T38VendorInfo = NULL;
    f_params->t38_options.T38FaxRateManagement = NULL;
}

/*============================================================================*/

static void fax_paramsDestroy(fax_params_t *f_params)
{
    if(f_params->pvt.t38_gw_state) free(f_params->pvt.t38_gw_state);
    if(f_params->pvt.udptl_state) free(f_params->pvt.udptl_state);

    if(f_params->pvt.header) free(f_params->pvt.header);
    if(f_params->pvt.ident) free(f_params->pvt.ident);

    if(f_params->t38_options.T38FaxUdpEC)
        free(f_params->t38_options.T38FaxUdpEC);
    if(f_params->t38_options.T38VendorInfo)
        free(f_params->t38_options.T38VendorInfo);
    if(f_params->t38_options.T38FaxRateManagement)
        free(f_params->t38_options.T38FaxRateManagement);
}

/*============================================================================*/

static void fax_paramsSetDefault(fax_params_t *f_params)
{
    session_t *session = f_params->session;

    f_params->pvt.disable_v17 = DEF_FAX_DISABLE_V17;
    f_params->pvt.done = 0;

    f_params->pvt.header = strdup(DEF_FAX_HEADER);
    f_params->pvt.verbose = DEF_FAX_VERBOSE;
    f_params->pvt.use_ecm = DEF_FAX_USE_ECM;

    f_params->t38_options.T38FaxVersion = DEF_T38_FAX_VERSION;
    f_params->t38_options.T38FaxMaxBuffer = DEF_T38_MAX_BUFFER;
    f_params->t38_options.T38FaxMaxDatagram = DEF_T38_MAX_DATAGRAM;
    f_params->t38_options.T38FaxRateManagement = strdup(DEF_T38_RATE_MANAGEMNT);
    f_params->t38_options.T38FaxTranscodingJBIG = DEF_T38_TRANSCODING_JBIG;
    f_params->t38_options.T38FaxTranscodingMMR = DEF_T38_TRANSCODING_MMR;
    f_params->t38_options.T38FaxUdpEC = strdup(DEF_T38_UDP_EC);
    f_params->t38_options.T38MaxBitRate = DEF_T38_MAX_BITRATE;
    f_params->t38_options.T38VendorInfo = strdup(DEF_T38_VENDOR_INFO);

    sprintf(f_params->log_tag, "%04x-%s", session->ses_id, session->call_id);
}

/*============================================================================*/











