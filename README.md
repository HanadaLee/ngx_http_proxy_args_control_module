# ngx_http_proxy_request_cookies_filter_module

# Name
ngx_http_proxy_request_cookies_filter_module

A NGINX module for fine-grained upstream request cookies control.

# Table of Content

- [ngx\_http\_proxy\_request\_cookies\_filter\_module](#ngx_http_proxy_request_cookies_filter_module)
- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Directives](#directives)
  - [proxy\_request\_cookies\_filter](#proxy_request_cookies_filter)
  - [proxy\_request\_cookies\_filter\_inherit](#proxy_request_cookies_filter_inherit)
- [Author](#author)
- [License](#license)

# Status

This Nginx module is currently considered experimental. Issues and PRs are welcome if you encounter any problems.

# Synopsis

```nginx
http {
    server {
        listen 80;
        server_name example.com;

        proxy_request_cookies_filter append form_server_level 1;

        location / {
            # Inherit all cookies from server. Default value is `on`.
            proxy_request_cookies_filter_inherit after;

            # If a cookie named "a" exists, set it to 1. Otherwise, add a cookie named "a" with value 1.
            proxy_request_cookies_filter set a 1;

            # If a cookie named "b" exists, do nothing. Otherwise, add a cookie named "a" with value 1.
            proxy_request_cookies_filter add b 2;

            # If a cookie named "c" exists, set it to 3. Otherwise, do nothing.
            proxy_request_cookies_filter rewrite c 3;
    
            # If a cookie named "d" exists, clear it. Otherwise, do nothing.
            proxy_request_cookies_filter clear d;

            # Clear all cookies.
            proxy_request_cookies_filter clear *;

            # Keep cookies. Other cookies will be cleared.
            proxy_request_cookies_filter keep e f g;

            # Conditional filtering. Only effected if varialbe $http_a is not empty or '0'.
            proxy_request_cookies_filter set h 4 if=$http_a;

            # If has `-i` option, the cookie name will be case-insensitive.
            proxy_request_cookies_filter set -i i 1;

            # If has `flag=break`, stop evaluating subsequent rules and output the final result.
            proxy_request_cookies_filter set j 5 flag=break;

            proxy_pass http://127.0.0.1:8080;
        }
    }
}
```

# Installation

This module requires [ngx_http_proxy_filter_module](https://github.com/your-repo/ngx_http_proxy_filter_module) to be compiled first.

To use theses modules, configure your nginx branch with:

```bash
./configure \
    --add-module=/path/to/ngx_http_proxy_filter_module \
    --add-module=/path/to/ngx_http_proxy_request_cookies_filter_module
```

# Directives

## proxy_request_cookies_filter

**Syntax:** `proxy_request_cookies_filter opeartor [-i] cookie_name value [flag=break] [if=condition];`

**Default:** —

**Context:** http, server, location

Filters cookies in the upstream request headers. All filter rules are applied in the order they are defined. The result directly modifies the `Cookie` header sent to the upstream.

The following operators are supported:

- `set`: Sets the value of a cookie. If the cookie already exists, it will be rewritten.
- `add`: Adds a new cookie. If the cookie already exists, the operation is ignored.
- `append`: Appends a new cookie even if the cookie already exists.
- `rewrite`: Rewrites the value of a cookie. If the cookie doesn't exist, the operation is ignored.
- `clear`: Removes a cookie from the request headers. If cookie name is `*`, all cookies will be cleared.
- `keep`: Keeps specified cookies. Other cookies will be cleared.

The following parameter are supported:

`-i` parameter makes the cookie name case-insensitive.
`flag=break` parameter makes the module stop evaluating subsequent rules and output the final result.
`if=condition` parameter makes the module evaluate the rule only if the condition value is not empty or '0'.

## proxy_request_cookies_filter_inherit

**Syntax:** `proxy_request_cookies_filter_inherit on | off | after | before`

**Default:** `proxy_request_cookies_filter_inherit on`

**Context:** http, server, location

Allows altering inheritance rules for the values specified in the `proxy_request_cookies_filter` directive. By default, the standard inheritance model is used.

The `before` parameter specifies that the rules inherited from the previous configuration level will be applied before the rules specified in the current location block.

the `after` parameter specifies that the rules inherited from the previous configuration level will be applied after the rules specified in the current location block.

The `off` parameter cancels inheritance of the values from the previous configuration level.

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
