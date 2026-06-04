# ngx_http_proxy_request_cookies_control_module

# Name
ngx_http_proxy_request_cookies_control_module

A NGINX module for fine-grained upstream request cookies control.

# Table of Content

- [ngx\_http\_proxy\_request\_cookies\_control\_module](#ngx_http_proxy_request_cookies_control_module)
- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Directives](#directives)
  - [proxy\_request\_cookies\_control](#proxy_request_cookies_control)
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

        proxy_request_cookies_control append form_server_level 1;

        location / {
            # If a cookie named "a" exists, set it to 1. Otherwise, add a cookie named "a" with value 1.
            proxy_request_cookies_control set a 1;

            # If a cookie named "b" exists, do nothing. Otherwise, add a cookie named "b" with value 2.
            proxy_request_cookies_control add b 2;

            # If a cookie named "c" exists, set it to 3. Otherwise, do nothing.
            proxy_request_cookies_control rewrite c 3;
    
            # If a cookie named "d" exists, clear it. Otherwise, do nothing.
            proxy_request_cookies_control clear d;

            # Clear all cookies.
            proxy_request_cookies_control clear *;

            # Clear cookies with a prefix.
            proxy_request_cookies_control clear session_*;

            # Keep cookies. Other cookies will be cleared.
            proxy_request_cookies_control keep e f g;

            # Pass a cookie through and disable later same-name rules.
            proxy_request_cookies_control pass token;

            # Conditional filtering. Only effected if varialbe $http_a is not empty or '0'.
            proxy_request_cookies_control set h 4 if=$http_a;

            # If has `-i` option, the cookie name will be case-insensitive.
            proxy_request_cookies_control set -i i 1;

            # If has `-b`, stop evaluating subsequent rules and output the final result.
            proxy_request_cookies_control set -b j 5;

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
    --add-module=/path/to/ngx_http_proxy_request_cookies_control_module
```

# Directives

## proxy_request_cookies_control

**Syntax:** `proxy_request_cookies_control operator [-i] [-b] cookie_name [value] [if=condition|if!=condition];`

**Default:** —

**Context:** http, server, location

Filters cookies in the upstream request headers. All filter rules are applied in the order they are defined. The result directly modifies the `Cookie` header sent to the upstream.

The following operators are supported:

- `set`: Sets the value of a cookie. If the cookie already exists, it will be rewritten.
- `add`: Adds a new cookie. If the cookie already exists, the operation is ignored.
- `append`: Appends a new cookie even if the cookie already exists.
- `rewrite`: Rewrites the value of a cookie. If the cookie doesn't exist, the operation is ignored.
- `clear`: Removes a cookie from the request headers. If cookie name is `*`, all cookies will be cleared. Prefix wildcards such as `session_*` are also supported.
- `keep`: Keeps specified cookies. Other cookies will be cleared.
- `pass`: No-op; explicitly passes a cookie through and disables later same-name rules.

The following parameter are supported:

`-i` parameter makes the cookie name case-insensitive.
`-b` parameter makes the module stop evaluating subsequent cookie rules and output the final result after the rule applies.
`if=condition` parameter makes the module evaluate the rule only if the condition value is not empty or '0'.

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
