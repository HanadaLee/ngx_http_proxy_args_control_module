
/*
 * Copyright (C) Hanada
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_proxy_filter_module.h>


#if !defined(NGX_HTTP_PROXY_FILTER) || !(NGX_HTTP_PROXY_FILTER)
#error "ngx_http_proxy_request_cookies_control_module requires NGX_HTTP_PROXY_FILTER"
#endif


typedef enum {
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_SET = 0,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_ADD,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_APPEND,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_REWRITE,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR_ALL,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP,
    NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_PASS
} ngx_http_proxy_request_cookies_control_opcode_e;


typedef struct {
    ngx_uint_t                               opcode;
    ngx_str_t                                name;
    ngx_array_t                             *name_list;
    ngx_http_complex_value_t                *value;
    ngx_http_complex_value_t                *filter;
    ngx_flag_t                               negative;
    ngx_flag_t                               ignore_case;
    ngx_flag_t                               break_flag;
    ngx_flag_t                               wildcard;
    ngx_uint_t                               id;
} ngx_http_proxy_request_cookies_control_rule_t;


typedef struct {
    ngx_array_t                             *rules;
    ngx_uint_t                               rules_cnt;
} ngx_http_proxy_request_cookies_control_loc_conf_t;


typedef struct {
    ngx_str_t                                name;
    ngx_str_t                                value;
    ngx_uint_t                               cleared;
} ngx_http_proxy_request_cookies_control_cookie_t;


typedef struct {
    ngx_uint_t                              *bits;
    ngx_uint_t                               size;
} ngx_http_proxy_request_cookies_control_bitmap_t;


static ngx_int_t ngx_http_proxy_request_cookies_control_filter(
    ngx_http_request_t *r, ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_request_cookies_control_init(ngx_conf_t *cf);

static ngx_int_t ngx_http_proxy_request_cookies_control_parse_cookie_value(
    ngx_http_request_t *r, ngx_str_t *src, ngx_array_t *cookies);

static ngx_int_t ngx_http_proxy_request_cookies_control_exec(
    ngx_http_request_t *r, ngx_list_t *headers, ngx_array_t *rules,
    ngx_uint_t rules_cnt);
static ngx_int_t ngx_http_proxy_request_cookies_control_parse_headers(
    ngx_http_request_t *r, ngx_list_t *headers, ngx_array_t *cookies,
    ngx_table_elt_t **cookie_header);
static ngx_int_t ngx_http_proxy_request_cookies_control_exec_rule(
    ngx_http_request_t *r, ngx_array_t **cookies,
    ngx_http_proxy_request_cookies_control_rule_t *rule,
    ngx_uint_t *changed);
static ngx_int_t ngx_http_proxy_request_cookies_control_rebuild_header(
    ngx_http_request_t *r, ngx_list_t *headers,
    ngx_table_elt_t *cookie_header, ngx_array_t *cookies);
static ngx_int_t ngx_http_proxy_request_cookies_control_is_cookie_header(
    ngx_table_elt_t *h);
static ngx_int_t ngx_http_proxy_request_cookies_control_match_rule_name(
    ngx_str_t *name, ngx_http_proxy_request_cookies_control_rule_t *rule);
static ngx_int_t ngx_http_proxy_request_cookies_control_match_name(
    ngx_str_t *name, ngx_str_t *pattern, ngx_flag_t ignore_case);

static char *ngx_http_proxy_request_cookies_control_directive(
    ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static void *ngx_http_proxy_request_cookies_control_create_loc_conf(
    ngx_conf_t *cf);
static char *ngx_http_proxy_request_cookies_control_merge_loc_conf(
    ngx_conf_t *cf, void *parent, void *child);

static ngx_int_t ngx_http_proxy_request_cookies_control_bitmap_init(
    ngx_http_proxy_request_cookies_control_bitmap_t *bm, ngx_uint_t size,
    ngx_pool_t *pool);
static void ngx_http_proxy_request_cookies_control_bitmap_set(
    ngx_http_proxy_request_cookies_control_bitmap_t *bm, ngx_uint_t bit);
static ngx_int_t ngx_http_proxy_request_cookies_control_bitmap_isset(
    ngx_http_proxy_request_cookies_control_bitmap_t *bm, ngx_uint_t bit);


static ngx_command_t  ngx_http_proxy_request_cookies_control_commands[] = {

    { ngx_string("proxy_request_cookie_control"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_2MORE,
      ngx_http_proxy_request_cookies_control_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proxy_request_cookies_control_module_ctx = {
    NULL,                                          /* preconfiguration */
    ngx_http_proxy_request_cookies_control_init,   /* postconfiguration */
    NULL,                                          /* create main conf */
    NULL,                                          /* init main conf */
    NULL,                                          /* create server conf */
    NULL,                                          /* merge server conf */
    ngx_http_proxy_request_cookies_control_create_loc_conf,
                                                   /* create loc conf */
    ngx_http_proxy_request_cookies_control_merge_loc_conf
                                                   /* merge loc conf */
};


ngx_module_t  ngx_http_proxy_request_cookies_control_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_request_cookies_control_module_ctx, /* module context */
    ngx_http_proxy_request_cookies_control_commands,    /* directives */
    NGX_HTTP_MODULE,                                    /* module type */
    NULL,                                               /* init master */
    NULL,                                               /* init module */
    NULL,                                               /* init process */
    NULL,                                               /* init thread */
    NULL,                                               /* exit thread */
    NULL,                                               /* exit process */
    NULL,                                               /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_proxy_request_cookies_control_bitmap_init(
    ngx_http_proxy_request_cookies_control_bitmap_t *bm, ngx_uint_t size,
    ngx_pool_t *pool)
{
    ngx_uint_t  n;

    bm->size = size;
    bm->bits = NULL;

    if (size == 0) {
        return NGX_OK;
    }

    n = (size + NGX_INT_T_LEN - 1) / NGX_INT_T_LEN;

    bm->bits = ngx_pcalloc(pool, n * sizeof(ngx_uint_t));
    if (bm->bits == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_proxy_request_cookies_control_bitmap_set(
    ngx_http_proxy_request_cookies_control_bitmap_t *bm, ngx_uint_t bit)
{
    if (bm->bits && bit < bm->size) {
        bm->bits[bit / NGX_INT_T_LEN]
            |= (ngx_uint_t) 1 << (bit % NGX_INT_T_LEN);
    }
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_bitmap_isset(
    ngx_http_proxy_request_cookies_control_bitmap_t *bm, ngx_uint_t bit)
{
    if (bm->bits == NULL || bit >= bm->size) {
        return NGX_DECLINED;
    }

    if ((bm->bits[bit / NGX_INT_T_LEN]
         & ((ngx_uint_t) 1 << (bit % NGX_INT_T_LEN)))
        == 0)
    {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_parse_cookie_value(
    ngx_http_request_t *r, ngx_str_t *src, ngx_array_t *cookies)
{
    u_char                               *start, *end, *last;
    ngx_str_t                             name, value;
    ngx_http_proxy_request_cookies_control_cookie_t  *cookie;

    if (src == NULL || src->len == 0) {
        return NGX_OK;
    }

    start = src->data;
    end = src->data + src->len;

    while (start < end) {

        while (start < end && (*start == ' ' || *start == ';')) {
            start++;
        }

        if (start == end) {
            break;
        }

        last = ngx_strlchr(start, end, '=');
        if (last == NULL) {
            break;
        }

        name.data = start;
        name.len = last - start;

        start = last + 1;
        last = ngx_strlchr(start, end, ';');
        if (last == NULL) {
            last = end;
        }

        value.data = start;
        value.len = last - start;

        cookie = ngx_array_push(cookies);
        if (cookie == NULL) {
            return NGX_ERROR;
        }

        cookie->name = name;
        cookie->value = value;
        cookie->cleared = 0;

        start = last;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_filter(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    ngx_int_t                                     rc;
    ngx_http_proxy_request_cookies_control_loc_conf_t  *clcf;

    if (ctx->headers == NULL) {
        return NGX_DECLINED;
    }

    clcf = ngx_http_get_module_loc_conf(r,
                              ngx_http_proxy_request_cookies_control_module);

    rc = ngx_http_proxy_request_cookies_control_exec(r, ctx->headers,
                                                     clcf->rules,
                                                     clcf->rules_cnt);
    if (rc != NGX_OK) {
        return rc;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_exec(ngx_http_request_t *r,
    ngx_list_t *headers, ngx_array_t *rules, ngx_uint_t rules_cnt)
{
    ngx_int_t                                       rc;
    ngx_uint_t                                      i, j;
    ngx_uint_t                                      changed, filtered;
    ngx_table_elt_t                                *cookie_header;
    ngx_array_t                                    *cookies;
    ngx_http_proxy_request_cookies_control_rule_t  *rule;
    ngx_http_proxy_request_cookies_control_bitmap_t locked;

    if (rules == NULL || rules->nelts == 0) {
        return NGX_DECLINED;
    }

    if (headers == NULL) {
        return NGX_ERROR;
    }

    cookies = ngx_array_create(r->pool, 4,
                    sizeof(ngx_http_proxy_request_cookies_control_cookie_t));
    if (cookies == NULL) {
        return NGX_ERROR;
    }

    cookie_header = NULL;
    if (ngx_http_proxy_request_cookies_control_parse_headers(r, headers,
                                                             cookies,
                                                             &cookie_header)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_http_proxy_request_cookies_control_bitmap_init(&locked, rules_cnt,
                                                           r->pool)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    filtered = 0;
    rule = rules->elts;

    for (i = 0; i < rules->nelts; i++) {

        if (!rule[i].wildcard
            && rule[i].opcode
               != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP)
        {
            for (j = 0; j < i; j++) {
                if (rule[j].wildcard
                    || rule[j].opcode
                       == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP)
                {
                    continue;
                }

                if (rule[i].name.len != rule[j].name.len) {
                    continue;
                }

                if (ngx_strncasecmp(rule[i].name.data, rule[j].name.data,
                                    rule[i].name.len)
                    != 0)
                {
                    continue;
                }

                if (ngx_http_proxy_request_cookies_control_bitmap_isset(
                        &locked, rule[j].id)
                    != NGX_OK)
                {
                    continue;
                }

                break;
            }

            if (j < i) {
                continue;
            }
        }

        changed = 0;
        rc = ngx_http_proxy_request_cookies_control_exec_rule(r, &cookies,
                                                              &rule[i],
                                                              &changed);

        if (rc == NGX_ERROR) {
            return rc;
        }

        if (rc == NGX_DECLINED) {
            continue;
        }

        if (changed) {
            filtered = 1;
        }

        if (!rule[i].wildcard
            && rule[i].opcode
               != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP)
        {
            ngx_http_proxy_request_cookies_control_bitmap_set(&locked,
                                                              rule[i].id);
        }

        if (rule[i].break_flag) {
            break;
        }
    }

    if (!filtered) {
        return NGX_DECLINED;
    }

    return ngx_http_proxy_request_cookies_control_rebuild_header(r, headers,
                                                                 cookie_header,
                                                                 cookies);
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_parse_headers(ngx_http_request_t *r,
    ngx_list_t *headers, ngx_array_t *cookies, ngx_table_elt_t **cookie_header)
{
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *h;

    *cookie_header = NULL;
    part = &headers->part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (ngx_http_proxy_request_cookies_control_is_cookie_header(&h[i])
            != NGX_OK)
        {
            continue;
        }

        if (*cookie_header == NULL) {
            *cookie_header = &h[i];
        }

        if (h[i].value.len == 0) {
            continue;
        }

        if (ngx_http_proxy_request_cookies_control_parse_cookie_value(
                r, &h[i].value, cookies)
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_exec_rule(ngx_http_request_t *r,
    ngx_array_t **cookies, ngx_http_proxy_request_cookies_control_rule_t *rule,
    ngx_uint_t *changed)
{
    ngx_str_t                                        value, *n;
    ngx_uint_t                                       i, j;
    ngx_uint_t                                       found, matched;
    ngx_array_t                                     *new_cookies;
    ngx_http_proxy_request_cookies_control_cookie_t *cookie, *new_cookie;

    *changed = 0;

    if (rule->filter) {
        if (ngx_http_complex_value(r, rule->filter, &value) != NGX_OK) {
            return NGX_ERROR;
        }

        if (value.len == 0 || (value.len == 1 && value.data[0] == '0')) {
            if (!rule->negative) {
                return NGX_DECLINED;
            }

        } else {
            if (rule->negative) {
                return NGX_DECLINED;
            }
        }
    }

    if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_PASS) {
        return NGX_OK;
    }

    cookie = (*cookies)->elts;

    if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR_ALL) {
        *changed = 1;

        for (i = 0; i < (*cookies)->nelts; i++) {
            if (cookie[i].cleared) {
                continue;
            }
            cookie[i].cleared = 1;
        }

        return NGX_OK;
    }

    if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP) {
        new_cookies = ngx_array_create(r->pool,
                                       ngx_max(rule->name_list->nelts, 4),
                  sizeof(ngx_http_proxy_request_cookies_control_cookie_t));
        if (new_cookies == NULL) {
            return NGX_ERROR;
        }

        for (i = 0; i < (*cookies)->nelts; i++) {
            if (cookie[i].cleared) {
                continue;
            }

            found = 0;

            for (j = 0; j < rule->name_list->nelts; j++) {
                n = (ngx_str_t *) rule->name_list->elts + j;

                if (ngx_http_proxy_request_cookies_control_match_name(
                        &cookie[i].name, n, rule->ignore_case)
                    == NGX_OK)
                {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                continue;
            }

            new_cookie = ngx_array_push(new_cookies);
            if (new_cookie == NULL) {
                return NGX_ERROR;
            }

            *new_cookie = cookie[i];
        }

        *cookies = new_cookies;

        *changed = 1;

        return NGX_OK;
    }

    if (rule->opcode != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR) {
        if (ngx_http_complex_value(r, rule->value, &value) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_APPEND) {
        if (value.len == 0) {
            return NGX_OK;
        }

        cookie = ngx_array_push(*cookies);
        if (cookie == NULL) {
            return NGX_ERROR;
        }

        cookie->name = rule->name;
        cookie->value = value;
        cookie->cleared = 0;
        *changed = 1;

        return NGX_OK;
    }

    found = 0;
    cookie = (*cookies)->elts;

    for (i = 0; i < (*cookies)->nelts; i++) {
        if (cookie[i].cleared) {
            continue;
        }

        if (ngx_http_proxy_request_cookies_control_match_rule_name(
                &cookie[i].name, rule)
            != NGX_OK)
        {
            continue;
        }

        if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_ADD) {
            found = 1;
            break;
        }

        matched = found;
        found = 1;

        if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR
            || value.len == 0
            || matched)
        {
            cookie[i].cleared = 1;
            *changed = 1;
            continue;
        }

        cookie[i].value = value;
        *changed = 1;
    }

    if (found) {
        return NGX_OK;
    }

    if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR
        || rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_REWRITE
        || value.len == 0)
    {
        return NGX_OK;
    }

    cookie = ngx_array_push(*cookies);
    if (cookie == NULL) {
        return NGX_ERROR;
    }

    cookie->name = rule->name;
    cookie->value = value;
    cookie->cleared = 0;
    *changed = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_rebuild_header(ngx_http_request_t *r,
    ngx_list_t *headers, ngx_table_elt_t *cookie_header, ngx_array_t *cookies)
{
    size_t                                           len;
    u_char                                          *p, *dst;
    ngx_uint_t                                       i, first;
    ngx_list_part_t                                 *part;
    ngx_table_elt_t                                 *h;
    ngx_http_proxy_request_cookies_control_cookie_t *cookie;

    len = 0;
    cookie = cookies->elts;

    for (i = 0; i < cookies->nelts; i++) {
        if (cookie[i].cleared) {
            continue;
        }

        len += cookie[i].name.len + 1 + cookie[i].value.len + 2;
    }

    if (len == 0) {
        part = &headers->part;
        h = part->elts;

        for (i = 0; /* void */; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                h = part->elts;
                i = 0;
            }

            if (ngx_http_proxy_request_cookies_control_is_cookie_header(&h[i])
                == NGX_OK)
            {
                h[i].hash = 0;
                h[i].value.len = 0;
                h[i].next = NULL;
            }
        }

        return NGX_OK;
    }

    len -= 2;

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    dst = p;
    first = 1;

    for (i = 0; i < cookies->nelts; i++) {
        if (cookie[i].cleared) {
            continue;
        }

        if (!first) {
            *p++ = ';';
            *p++ = ' ';
        }

        p = ngx_copy(p, cookie[i].name.data, cookie[i].name.len);
        *p++ = '=';
        p = ngx_copy(p, cookie[i].value.data, cookie[i].value.len);

        first = 0;
    }

    if (cookie_header == NULL) {
        cookie_header = ngx_list_push(headers);
        if (cookie_header == NULL) {
            return NGX_ERROR;
        }

        ngx_memzero(cookie_header, sizeof(ngx_table_elt_t));

        ngx_str_set(&cookie_header->key, "Cookie");
        cookie_header->lowcase_key = (u_char *) "cookie";
    }

    cookie_header->hash = 1;
    cookie_header->value.data = dst;
    cookie_header->value.len = len;
    cookie_header->next = NULL;

    part = &headers->part;
    h = part->elts;

    for (i = 0; /* void */; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (&h[i] == cookie_header) {
            continue;
        }

        if (ngx_http_proxy_request_cookies_control_is_cookie_header(&h[i])
            == NGX_OK)
        {
            h[i].hash = 0;
            h[i].value.len = 0;
            h[i].next = NULL;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_is_cookie_header(ngx_table_elt_t *h)
{
    if (h->hash == 0) {
        return NGX_DECLINED;
    }

    if (h->key.len != sizeof("Cookie") - 1) {
        return NGX_DECLINED;
    }

    if (ngx_strncasecmp(h->key.data, (u_char *) "Cookie",
                        sizeof("Cookie") - 1)
        != 0)
    {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_match_rule_name(ngx_str_t *name,
    ngx_http_proxy_request_cookies_control_rule_t *rule)
{
    ngx_str_t  pattern;

    pattern = rule->name;

    if (rule->wildcard) {
        pattern.len--;

        if (name->len < pattern.len) {
            return NGX_DECLINED;
        }

        if (rule->ignore_case) {
            if (ngx_strncasecmp(name->data, pattern.data, pattern.len) != 0) {
                return NGX_DECLINED;
            }

            return NGX_OK;
        }

        if (ngx_strncmp(name->data, pattern.data, pattern.len) != 0) {
            return NGX_DECLINED;
        }

        return NGX_OK;
    }

    return ngx_http_proxy_request_cookies_control_match_name(name, &pattern,
                                                            rule->ignore_case);
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_match_name(ngx_str_t *name,
    ngx_str_t *pattern, ngx_flag_t ignore_case)
{
    if (name->len != pattern->len) {
        return NGX_DECLINED;
    }

    if (ignore_case) {
        if (ngx_strncasecmp(name->data, pattern->data, pattern->len) != 0) {
            return NGX_DECLINED;
        }

        return NGX_OK;
    }

    if (ngx_strncmp(name->data, pattern->data, pattern->len) != 0) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_request_cookies_control_init(ngx_conf_t *cf)
{
    ngx_http_proxy_filter_pt          *h;
    ngx_http_proxy_filter_main_conf_t *pmcf;

    pmcf = ngx_http_conf_get_module_main_conf(cf,
                                              ngx_http_proxy_filter_module);
    if (pmcf == NULL) {
        return NGX_ERROR;
    }

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_REQUEST_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_proxy_request_cookies_control_filter;

    return NGX_OK;
}


static char *
ngx_http_proxy_request_cookies_control_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_request_cookies_control_loc_conf_t *clcf = conf;

    ngx_str_t                                        *arg, s, *n;
    ngx_uint_t                                        cur;
    ngx_http_compile_complex_value_t                  ccv;
    ngx_http_proxy_request_cookies_control_rule_t    *rule;

    arg = cf->args->elts;

    if (cf->args->nelts < 3) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" requires at least 2 arguments",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    if (clcf->rules == NULL) {
        clcf->rules = ngx_array_create(cf->pool, 4,
                    sizeof(ngx_http_proxy_request_cookies_control_rule_t));
        if (clcf->rules == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    rule = ngx_array_push(clcf->rules);
    if (rule == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(rule,
                sizeof(ngx_http_proxy_request_cookies_control_rule_t));

    /* parse operation */

    if (arg[1].len == 3
        && ngx_strncasecmp(arg[1].data, (u_char *) "set", 3) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_SET;

    } else if (arg[1].len == 3
               && ngx_strncasecmp(arg[1].data, (u_char *) "add", 3) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_ADD;

    } else if (arg[1].len == 6
               && ngx_strncasecmp(arg[1].data, (u_char *) "append", 6) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_APPEND;

    } else if (arg[1].len == 7
               && ngx_strncasecmp(arg[1].data, (u_char *) "rewrite", 7) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_REWRITE;

    } else if (arg[1].len == 5
               && ngx_strncasecmp(arg[1].data, (u_char *) "clear", 5) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR;

    } else if (arg[1].len == 4
               && ngx_strncasecmp(arg[1].data, (u_char *) "keep", 4) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP;

    } else if (arg[1].len == 4
               && ngx_strncasecmp(arg[1].data, (u_char *) "pass", 4) == 0)
    {
        rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_PASS;

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid operation \"%V\"", &arg[1]);
        return NGX_CONF_ERROR;
    }

    cur = 2;

    /* parse -i and -b */

    for ( ;; ) {

        if (cf->args->nelts > cur
            && arg[cur].len == 2
            && ngx_strncmp(arg[cur].data, "-i", 2) == 0)
        {
            rule->ignore_case = 1;
            cur++;
            continue;
        }

        if (cf->args->nelts > cur
            && arg[cur].len == 2
            && ngx_strncmp(arg[cur].data, "-b", 2) == 0)
        {
            rule->break_flag = 1;
            cur++;
            continue;
        }

        break;
    }

    /* parse cookie name or name_list */

    if (rule->opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP) {

        if (cf->args->nelts <= cur) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"%V\" keep requires a name list",
                               &cmd->name);
            return NGX_CONF_ERROR;
        }

        for ( /* void */ ; cur < cf->args->nelts; cur++) {

            if (arg[cur].len == 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "cookie name is empty");
                return NGX_CONF_ERROR;
            }

            if (arg[cur].len > 3
                && ngx_strncmp(arg[cur].data, "if=", 3) == 0)
            {
                break;
            }

            if (arg[cur].len > 4
                && ngx_strncmp(arg[cur].data, "if!=", 4) == 0)
            {
                break;
            }

            if (rule->name_list == NULL) {
                rule->name_list = ngx_array_create(cf->pool, 4,
                                                   sizeof(ngx_str_t));
                if (rule->name_list == NULL) {
                    return NGX_CONF_ERROR;
                }
            }

            n = ngx_array_push(rule->name_list);
            if (n == NULL) {
                return NGX_CONF_ERROR;
            }

            *n = arg[cur];
        }

        if (rule->name_list == NULL || rule->name_list->nelts == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"%V\" keep requires a name list",
                               &cmd->name);
            return NGX_CONF_ERROR;
        }

        if (cur >= cf->args->nelts) {
            rule->id = clcf->rules_cnt++;
            return NGX_CONF_OK;
        }

    } else {

        if (cf->args->nelts <= cur) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"%V\" requires a cookie name", &cmd->name);
            return NGX_CONF_ERROR;
        }

        rule->name = arg[cur];

        if (rule->name.len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "cookie name is empty");
            return NGX_CONF_ERROR;
        }

        rule->wildcard = (rule->name.data[rule->name.len - 1] == '*');

        if (rule->wildcard) {
            if (rule->opcode != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "\"%V\" wildcard is only supported by clear",
                                   &cmd->name);
                return NGX_CONF_ERROR;
            }

            if (rule->name.len == 1) {
                rule->opcode = NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR_ALL;
            }
        }

        cur++;

        /* parse and compile value */

        if (rule->opcode != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR
            && rule->opcode != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_CLEAR_ALL
            && rule->opcode != NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_PASS)
        {
            if (cf->args->nelts <= cur) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "\"%V\" %V requires a value",
                                   &cmd->name, &arg[1]);
                return NGX_CONF_ERROR;
            }

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &arg[cur];
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_http_complex_value_t));
            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            rule->value = ccv.complex_value;
            cur++;
        }
    }

    /* parse if= / if!= */

    if (cf->args->nelts > cur) {

        if (arg[cur].len > 3
            && ngx_strncmp(arg[cur].data, "if=", 3) == 0)
        {
            rule->negative = 0;
            s.len = arg[cur].len - 3;
            s.data = arg[cur].data + 3;

        } else if (arg[cur].len > 4
                   && ngx_strncmp(arg[cur].data, "if!=", 4) == 0)
        {
            rule->negative = 1;
            s.len = arg[cur].len - 4;
            s.data = arg[cur].data + 4;

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &arg[cur]);
            return NGX_CONF_ERROR;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &s;
        ccv.complex_value = ngx_palloc(cf->pool,
                                       sizeof(ngx_http_complex_value_t));
        if (ccv.complex_value == NULL) {
            return NGX_CONF_ERROR;
        }

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        rule->filter = ccv.complex_value;
        cur++;
    }

    if (cf->args->nelts > cur) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &arg[cur]);
        return NGX_CONF_ERROR;
    }

    rule->id = clcf->rules_cnt++;

    return NGX_CONF_OK;
}


static void *
ngx_http_proxy_request_cookies_control_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_request_cookies_control_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool,
                    sizeof(ngx_http_proxy_request_cookies_control_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_http_proxy_request_cookies_control_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child)
{
    ngx_http_proxy_request_cookies_control_loc_conf_t  *prev = parent;
    ngx_http_proxy_request_cookies_control_loc_conf_t  *conf = child;
    ngx_uint_t                                         i, j;
    ngx_uint_t                                         orig_len, prev_len;
    ngx_uint_t                                         copy_count, pos;
    ngx_http_proxy_request_cookies_control_rule_t     *prev_r, *r;
    ngx_http_proxy_request_cookies_control_bitmap_t    disable_map;

    if (conf->rules == NULL || conf->rules->nelts == 0) {
        conf->rules = prev->rules;
        conf->rules_cnt = prev->rules_cnt;

    } else if (prev->rules && prev->rules->nelts) {
        orig_len = conf->rules->nelts;
        prev_len = prev->rules->nelts;

        r = conf->rules->elts;
        prev_r = prev->rules->elts;

        if (ngx_http_proxy_request_cookies_control_bitmap_init(
                &disable_map, prev->rules_cnt, cf->pool)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }

        for (i = 0; i < orig_len; i++) {

            if (r[i].filter
                || r[i].opcode == NGX_HTTP_PROXY_REQUEST_COOKIES_CONTROL_KEEP
                || r[i].wildcard)
            {
                continue;
            }

            for (j = 0; j < prev_len; j++) {

                if (prev_r[j].wildcard) {
                    continue;
                }

                if (r[i].name.len != prev_r[j].name.len) {
                    continue;
                }

                if (ngx_strncasecmp(r[i].name.data, prev_r[j].name.data,
                                    r[i].name.len)
                    != 0)
                {
                    continue;
                }

                ngx_http_proxy_request_cookies_control_bitmap_set(&disable_map,
                                                                  prev_r[j].id);
            }
        }

        copy_count = 0;

        for (j = 0; j < prev_len; j++) {
            if (ngx_http_proxy_request_cookies_control_bitmap_isset(
                    &disable_map, prev_r[j].id)
                != NGX_OK)
            {
                copy_count++;
            }
        }

        if (copy_count > 0) {
            if (ngx_array_push_n(conf->rules, copy_count) == NULL) {
                return NGX_CONF_ERROR;
            }

            r = conf->rules->elts;
            pos = orig_len;

            for (j = 0; j < prev_len; j++) {
                if (ngx_http_proxy_request_cookies_control_bitmap_isset(
                        &disable_map, prev_r[j].id)
                    != NGX_OK)
                {
                    r[pos++] = prev_r[j];
                }
            }
        }

        r = conf->rules->elts;

        for (i = 0; i < conf->rules->nelts; i++) {
            r[i].id = i;
        }

        conf->rules_cnt = conf->rules->nelts;
    }

    return NGX_CONF_OK;
}
