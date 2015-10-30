#include <nginx.h>
#include <ngx_http.h>
#include <ngx_http_variables.h>

static u_int   txserial_last_odd = 1;
static u_int   txserial_last_even = 0;

static ngx_int_t ngx_txserial_add_variables();
static void * ngx_txserial_create_loc_conf(ngx_conf_t *);
static char * ngx_txserial_merge_loc_conf(ngx_conf_t *, void *, void *);
static ngx_int_t ngx_txserial_init_process(ngx_cycle_t *);
static void ngx_txserial_exit_process(ngx_cycle_t *);

typedef struct {
    ngx_uint_t   txstart;
    ngx_uint_t   txend;
    ngx_uint_t   txincr;
    ngx_uint_t   txbmon;
    ngx_uint_t   txbase;
} ngx_txserial_loc_conf_t;

static ngx_http_module_t  ngx_txserial_module_ctx = {
  ngx_txserial_add_variables,  /* preconfiguration */
  NULL,                        /* postconfiguration */

  NULL,        /* create main configuration */
  NULL,        /* init main configuration */

  NULL,        /* create server configuration */
  NULL,        /* merge server configuration */

  ngx_txserial_create_loc_conf,  /* create location configuration */
  ngx_txserial_merge_loc_conf    /* merge location configuration */
};

static ngx_command_t  ngx_txserial_module_commands[] = {
  { ngx_string("txstart"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_txserial_loc_conf_t, txstart),
      NULL },
  { ngx_string("txend"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_txserial_loc_conf_t, txend),
      NULL },
  { ngx_string("txincr"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_txserial_loc_conf_t, txincr),
      NULL },
  { ngx_string("txbmon"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_txserial_loc_conf_t, txbmon),
      NULL },
  { ngx_string("txbase"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_txserial_loc_conf_t, txbase),
      NULL },

  ngx_null_command
};

ngx_module_t  ngx_txserial_module = {
  NGX_MODULE_V1,
  &ngx_txserial_module_ctx,      /* module context */
  ngx_txserial_module_commands,  /* module directives */
  NGX_HTTP_MODULE,                /* module type */
  NULL,                           /* init master */
  NULL,                           /* init module */
  ngx_txserial_init_process,      /* init process */
  NULL,                           /* init thread */
  NULL,                           /* exit thread */
  ngx_txserial_exit_process,      /* exit process */
  NULL,                           /* exit master */
  NGX_MODULE_V1_PADDING
};


// returns time in seconds since 1973
static ngx_int_t
ngx_txsec_get(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
    u_char      *p;
    ngx_time_t  *tp;

    p = ngx_pnalloc(r->pool, NGX_TIME_T_LEN + 4);
    if (p == NULL) {
        return NGX_ERROR;
    }

    tp = ngx_timeofday();

    v->len = ngx_sprintf(p, "%T", tp->sec) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

// uses a static global txserial_last_odd to count
static ngx_int_t
ngx_txodd_get(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
    
    ngx_txserial_loc_conf_t  *conf;
    conf = ngx_http_get_module_loc_conf(r, ngx_txserial_module); 

    if (txserial_last_odd < conf->txend) {
        txserial_last_odd += conf->txincr;
    } else {
        txserial_last_odd = conf->txstart;
    }

    size_t enclen = 6;  /* 499,999 == 7A11F + \0 */

    u_char *out = ngx_pnalloc(r->pool, enclen);
    if (out == NULL) {
        v->valid = 0;
        v->not_found = 1;
        return NGX_ERROR;
    }

    snprintf((char *)out, enclen, "%05x", (unsigned int)txserial_last_odd);

    v->len = enclen-1;
    v->data = out;

    v->valid = 1;
    v->not_found = 0;
    v->no_cacheable = 0;

    return NGX_OK;
}

// uses a static global txserial_last_odd to count
static ngx_int_t
ngx_txeven_get(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {

    txserial_last_even = txserial_last_odd + 1;

    size_t enclen = 6;  /* 499,999 == 7A11F + \0 */

    u_char *out = ngx_pnalloc(r->pool, enclen);
    if (out == NULL) {
        v->valid = 0;
        v->not_found = 1;
        return NGX_ERROR;
    }

    snprintf((char *)out, enclen, "%05x", (unsigned int)txserial_last_even);

    v->len = enclen-1;
    v->data = out;

    v->valid = 1;
    v->not_found = 0;
    v->no_cacheable = 0;

    return NGX_OK;
}

static ngx_int_t
ngx_txweek_get(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {

    u_char      *p;
    ngx_time_t  *tp;

    ngx_txserial_loc_conf_t  *conf;
    conf = ngx_http_get_module_loc_conf(r, ngx_txserial_module); 

    p = ngx_pnalloc(r->pool, NGX_TIME_T_LEN + 4);
    if (p == NULL) {
        return NGX_ERROR;
    }

    tp = ngx_timeofday();

    /*
     * adjust the i value for offset in weeks from the initialization date
     */
    const int aweek = 86400 * 7;

    /*
     * the first monday which defines the start of the counting sequence in seconds
     * Mon 23 Sep 2013 00:00:00 UTC in seconds since 1970 = 1379894400
     */
    const int bmon = conf->txbmon;
    const int base = conf->txbase;

    int week = base + (tp->sec - bmon) / aweek;

    v->len = snprintf((char *)p, NGX_TIME_T_LEN + 4, "%4d", week);
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

static ngx_int_t
ngx_txserial_init_process(ngx_cycle_t *cycle) {
    return NGX_OK;
}

static void
ngx_txserial_exit_process(ngx_cycle_t *cycle) {
}

static void *
ngx_txserial_create_loc_conf(ngx_conf_t *cf)
{
    ngx_txserial_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_txserial_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    conf->txstart = NGX_CONF_UNSET_UINT;
    conf->txend = NGX_CONF_UNSET_UINT;
    conf->txbmon = NGX_CONF_UNSET_UINT;
    conf->txbase = NGX_CONF_UNSET_UINT;
    conf->txincr = NGX_CONF_UNSET_UINT;
    return conf;
}

static char *
ngx_txserial_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_txserial_loc_conf_t *prev = parent;
    ngx_txserial_loc_conf_t *conf = child;

    ngx_conf_merge_uint_value(conf->txstart, prev->txstart, 1);
    ngx_conf_merge_uint_value(conf->txend, prev->txend, 499999);
    ngx_conf_merge_uint_value(conf->txincr, prev->txincr, 2);
    ngx_conf_merge_uint_value(conf->txbmon, prev->txbmon, 1379894400);
    ngx_conf_merge_uint_value(conf->txbase, prev->txbase, 5000);

    if (conf->txstart + conf->txincr > conf->txend) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "txstart plus txincr must be less than txend"); 
        return NGX_CONF_ERROR;
    }

    if ((conf->txstart & 0x1) != 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "txstart must be odd"); 
        return NGX_CONF_ERROR;
    }

    if ((conf->txend & 0x1) != 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "txend must be odd"); 
        return NGX_CONF_ERROR;
    }

    if (conf->txend < conf->txstart) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "txend must be greater than txstart"); 
        return NGX_CONF_ERROR;
    }

    if (conf->txbmon < 1379894400) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "txbmon must be equal or more than 1379894400"); 
        return NGX_CONF_ERROR;
    }

    /* forces next iteration to jump to the start */
    txserial_last_odd = conf->txend;
    return NGX_CONF_OK;
}

static ngx_str_t ngx_txodd_variable_name = ngx_string("txodd");
static ngx_str_t ngx_txeven_variable_name = ngx_string("txeven");
static ngx_str_t ngx_txsec_variable_name = ngx_string("txsec");
static ngx_str_t ngx_txweek_variable_name = ngx_string("txweek");

static ngx_int_t ngx_txserial_add_variables(ngx_conf_t *cf)
{
  ngx_http_variable_t* vartx = ngx_http_add_variable(
          cf,
          &ngx_txodd_variable_name,
          NGX_HTTP_VAR_NOHASH);

  if (vartx == NULL) {
      return NGX_ERROR;
  }

  vartx->get_handler = ngx_txodd_get;

  ngx_http_variable_t* vartb = ngx_http_add_variable(
          cf,
          &ngx_txeven_variable_name,
          NGX_HTTP_VAR_NOHASH);

  if (vartb == NULL) {
      return NGX_ERROR;
  }

  vartb->get_handler = ngx_txeven_get;

  ngx_http_variable_t* vartxs = ngx_http_add_variable(
          cf,
          &ngx_txsec_variable_name,
          NGX_HTTP_VAR_NOHASH);

  if (vartxs == NULL) {
      return NGX_ERROR;
  }

  vartxs->get_handler = ngx_txsec_get;

  ngx_http_variable_t* vartxw = ngx_http_add_variable(
          cf,
          &ngx_txweek_variable_name,
          NGX_HTTP_VAR_NOHASH);

  if (vartxw == NULL) {
      return NGX_ERROR;
  }

  vartxw->get_handler = ngx_txweek_get;

  return NGX_OK;
}

