#!/usr/bin/perl

# Tests for ngx_http_proxy_args_control_module.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use Test::Nginx qw/ :DEFAULT http_content /;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http proxy rewrite ngx_condition_module
	ngx_http_proxy_filter_module ngx_http_proxy_args_control_module/)
	->plan(25);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    server {
        listen       127.0.0.1:8081;
        server_name  backend;

        location / {
            return 200 $request_uri;
        }
    }

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;

        condition selected str_eq $arg_mode hit;

        location = /set {
            proxy_arg_control set a new;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /add {
            proxy_arg_control add a new;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /append {
            proxy_arg_control append c $arg_v;
            proxy_arg_control clear v;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /rewrite {
            proxy_arg_control rewrite a new;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /clear {
            proxy_arg_control clear a;
            proxy_arg_control clear utm_*;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /empty {
            proxy_arg_control clear '';
            proxy_pass http://127.0.0.1:8081;
        }

        location = /clear-all {
            proxy_arg_control clear *;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /keep {
            proxy_arg_control keep a flag;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /keep-insensitive {
            proxy_arg_control keep -i a;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /pass {
            proxy_arg_control pass token;
            proxy_arg_control set token replaced;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /next {
            proxy_arg_control set -n a one;
            proxy_arg_control set a two;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /break {
            proxy_arg_control set -b a one;
            proxy_arg_control set b two;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /insensitive {
            proxy_arg_control set -i Foo new;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /case-rules {
            proxy_arg_control set Foo upper;
            proxy_arg_control set foo lower;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /conditional {
            when selected {
                proxy_arg_control set result selected;
            }

            proxy_arg_control set result fallback;
            proxy_arg_control clear mode;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /order {
            proxy_arg_control set result first;

            when selected {
                proxy_arg_control set result second;
            }

            proxy_arg_control clear mode;
            proxy_pass http://127.0.0.1:8081;
        }

        location /inherit/ {
            proxy_arg_control append parent p;

            location = /inherit/test {
                proxy_arg_control append child c;
                proxy_pass http://127.0.0.1:8081;
            }
        }

        location /inherit-case/ {
            proxy_arg_control set Foo parent;

            location = /inherit-case/test {
                proxy_arg_control set foo child;
                proxy_pass http://127.0.0.1:8081;
            }
        }

        location = /plain {
            proxy_pass http://127.0.0.1:8081;
        }
    }
}

EOF

$t->run();

###############################################################################

is(body('/set?a=old&a=second&b=2'), '/set?a=new&b=2',
	'set rewrites the first value and removes duplicates');
is(body('/set?b=2'), '/set?b=2&a=new', 'set adds a missing argument');

is(body('/add?a=old&b=2'), '/add?a=old&b=2',
	'add preserves an existing argument');
is(body('/add?b=2'), '/add?b=2&a=new', 'add creates a missing argument');

is(body('/append?a=1&v=dynamic'), '/append?a=1&c=dynamic',
	'append expands variables and adds a duplicate-capable argument');

is(body('/rewrite?a=old&a=second&b=2'), '/rewrite?a=new&b=2',
	'rewrite changes an existing argument and removes duplicates');
is(body('/rewrite?b=2'), '/rewrite?b=2',
	'rewrite does not create a missing argument');

is(body('/clear?a=1&utm_source=x&keep=2&utm_medium=y'), '/clear?keep=2',
	'clear removes named and prefix-wildcard arguments');
is(body('/empty?=one&&a=1&='), '/empty?a=1',
	'clear supports empty argument keys and empty segments');
is(body('/clear-all?a=1&b=2'), '/clear-all',
	'clear wildcard removes every argument');

is(body('/keep?a=1&drop=2&flag&a=3'), '/keep?a=1&flag&a=3',
	'keep preserves matching arguments, order, duplicates and missing equals');
is(body('/keep-insensitive?A=1&b=2'), '/keep-insensitive?A=1',
	'keep supports case-insensitive names');

is(body('/pass?token=original'), '/pass?token=original',
	'pass locks the name against later rules');
is(body('/next?a=old'), '/next?a=two',
	'next permits a later same-name rule');
is(body('/break?a=old'), '/break?a=one',
	'break stops processing subsequent rules');

is(body('/insensitive?fOo=old'), '/insensitive?fOo=new',
	'case-insensitive set preserves the original name');
is(body('/case-rules?Foo=old&foo=old'),
	'/case-rules?Foo=upper&foo=lower',
	'distinct case-sensitive rule names are evaluated independently');

is(body('/conditional?mode=hit&result=old'),
	'/conditional?result=selected',
	'matching condition wins in configuration order');
is(body('/conditional?mode=miss&result=old'),
	'/conditional?result=fallback',
	'condition miss falls through to the unconditional rule');
is(body('/order?mode=hit&result=old'), '/order?result=first',
	'first unconditional rule wins over a later matching condition');

is(body('/inherit/test?a=1'), '/inherit/test?a=1&child=c&parent=p',
	'child rules run before inherited parent rules');
is(body('/inherit-case/test?Foo=old&foo=old'),
	'/inherit-case/test?Foo=parent&foo=child',
	'case-sensitive parent and child rule names merge independently');
is(body('/plain?a=1&&flag'), '/plain?a=1&&flag',
	'location without rules passes the query unchanged');

is(body('/set'), '/set?a=new', 'set creates a query string on a bare URI');
is(body('/set?a='), '/set?a=new', 'set rewrites an empty argument value');

###############################################################################

sub body {
	my ($uri) = @_;

	return http_content(http_get($uri));
}

###############################################################################
