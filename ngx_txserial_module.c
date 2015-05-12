#include <nginx.h>
#include <ngx_http.h>
#include <ngx_http_variables.h>

static u_int   txserial_last_odd = 1;
static u_int   txserial_last_even = 0;

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

    if (txserial_last_odd < 499999) {
        txserial_last_odd += 2;
    } else {
        txserial_last_odd = 1;
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
    const int bmon = 1379894400;
    const int base = 5000;

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

static ngx_http_module_t  ngx_txserial_module_ctx = {
  ngx_txserial_add_variables,  /* preconfiguration */
  NULL,                        /* postconfiguration */

  NULL,        /* create main configuration */
  NULL,        /* init main configuration */

  NULL,        /* create server configuration */
  NULL,        /* merge server configuration */

  NULL,        /* create location configuration */
  NULL         /* merge location configuration */
};

static ngx_command_t  ngx_txserial_module_commands[] = {
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

