# ngx_http_proxy_args_control_module

# Name
ngx_http_proxy_args_control_module

An NGINX module for fine-grained upstream proxy URI query argument control.

# Table of Content

- [ngx\_http\_proxy\_args\_control\_module](#ngx_http_proxy_args_control_module)
- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Conditional syntax](#conditional-syntax)
- [Directives](#directives)
  - [proxy\_arg\_control](#proxy_arg_control)
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

        proxy_arg_control append form_server_level 1;

        location / {
            # If an argument named "a" exists, set it to 1. Otherwise, add an argument named "a" with value 1.
            proxy_arg_control set a 1;

            # If an argument named "b" exists, do nothing. Otherwise, add an argument named "b" with value 2.
            proxy_arg_control add b 2;

            # If an argument named "c" exists, set it to 3. Otherwise, do nothing.
            proxy_arg_control rewrite c 3;
    
            # If an argument named "d" exists, clear it. Otherwise, do nothing.
            proxy_arg_control clear d;

            # Clear all arguments.
            proxy_arg_control clear *;

            # Clear arguments with a prefix.
            proxy_arg_control clear utm_*;

            # Clear arguments with an empty key, such as "=value" or an empty segment in "a=1&&b=2".
            proxy_arg_control clear '';

            # Keep arguments. Other arguments will be cleared.
            proxy_arg_control keep e f g;

            # Pass an argument through and disable later same-name rules.
            proxy_arg_control pass token;

            # With ngx_condition_module.
            condition has_header_a is_not_empty $http_a;
            when has_header_a {
                proxy_arg_control set h 4;
            }

            # With the `-i` option, the argument name will be case-insensitive.
            proxy_arg_control set -i i 1;

            # With `-b`, stop evaluating subsequent rules and output the final result.
            proxy_arg_control set -b j 5;

            proxy_pass http://127.0.0.1:8080;
        }
    }
}
```

# Installation

This module requires [ngx_http_proxy_filter_module](https://github.com/your-repo/ngx_http_proxy_filter_module) to be compiled first.

To use these modules, configure your nginx branch with:

```bash
./configure \
    --add-module=/path/to/ngx_http_proxy_filter_module \
    --add-module=/path/to/ngx_http_proxy_args_control_module
```

To enable named conditions, add `--add-module=/path/to/ngx_condition_module` to the same static nginx build.

# Conditional syntax

Conditional syntax is selected at compile time:

- With `ngx_condition_module`, use named `condition` expressions and place `proxy_arg_control` inside an `http`, `server`, or `location` `when` block. `if=` and `if!=` parameters are rejected.
- Without `ngx_condition_module`, `when` is unavailable and legacy `if=`/`if!=` parameters remain supported. `if=` matches a non-empty value other than `"0"`; `if!=` matches an empty value or `"0"`.

If a condition does not match, the rule is skipped and does not stop later rules for the same argument. The directive also remains valid in nginx's native `if` block inside a location; that context is separate from a condition-module `when` block.

# Directives

## proxy_arg_control

**Syntax:** `proxy_arg_control operator [-i] [-n] [-b] arg_name [value ...];`

**Default:** —

**Context:** http, server, location, location if, http when, server when, location when

Controls the query arguments in the upstream proxy URI. The module reads the full proxy URI, applies rules to the query string, rebuilds the URI, and writes it back before proxying.

The following operators are supported:

| Operator  | Description                                                                                                           |
|-----------|-----------------------------------------------------------------------------------------------------------------------|
| `set`     | Sets the value of an argument. If the argument already exists, it will be rewritten.                                       |
| `add`     | Adds a new argument. If the argument already exists, the operation is ignored.                                            |
| `append`  | Appends a new argument even if the argument already exists.                                                               |
| `rewrite` | Rewrites the value of an argument. If the argument doesn't exist, the operation is ignored.                                |
| `clear`   | Removes an argument from the proxy URI. Prefix wildcards such as `utm_*` are supported. If argument name is `*`, all arguments will be cleared. |
| `keep`    | Keeps specified arguments. Multiple argument names can be provided. Other arguments will be cleared.                        |
| `pass`    | No-op; explicitly passes an argument through and disables later same-name rules.                                         |

Use `''` as the argument name to match arguments with an empty key, including `=value`, `=`, and empty segments produced by leading, trailing, or adjacent `&` characters.

The following parameters are supported:

| Parameter | Description |
|-----------|-------------|
| `-i` | Makes the argument name case-insensitive. When an existing argument is matched and needs to be set or rewritten, only its value is modified. The original name is preserved. |
| `-n` | By default, once an argument name is matched, subsequent rules for that name are skipped. This flag continues evaluating later rules for the same argument name after this rule is applied. Wildcard `clear` and `keep` rules are not affected and always continue. |
| `-b` | Stops evaluating subsequent argument rules and outputs the final result after this rule applies. |
| `if=condition` | Legacy-only: evaluates the rule if the value is non-empty and not `0`. |
| `if!=condition` | Legacy-only: evaluates the rule if the value is empty or `0`. |

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
