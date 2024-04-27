/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2017-2024, plures
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <ndtypes.h>

#include "test.h"
#include "alloc_fail.h"


static int
init_tests(void)
{
    ndt_context_t *ctx;
    const ndt_t *t = NULL;
    int ret;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    if (ndt_init(ctx) < 0) {
        ndt_err_fprint(stderr, ctx);
        ndt_context_del(ctx);
        return -1;
    }

    t = ndt_from_string("{a: size_t, b: ref(string)}", ctx);
    if (t == NULL) {
        ndt_err_fprint(stderr, ctx);
        ndt_context_del(ctx);
        return -1;
    }

    ret = ndt_typedef("defined_t", t, NULL, ctx);
    ndt_decref(t);
    if (ret < 0) {
        ndt_err_fprint(stderr, ctx);
        ndt_context_del(ctx);
        return -1;
    }

    t = ndt_from_string("(10 * 2 * defined_t)", ctx);
    if (t == NULL) {
        ndt_err_fprint(stderr, ctx);
        ndt_context_del(ctx);
        return -1;
    }

    ret = ndt_typedef("foo_t", t, NULL, ctx);
    ndt_decref(t);
    if (ret < 0) {
        ndt_err_fprint(stderr, ctx);
        ndt_context_del(ctx);
        return -1;
    }

    ndt_context_del(ctx);
    return 0;
}

static int
test_parse(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    char *s;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = parse_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            t = ndt_from_string(*c, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (t != NULL) {
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_parse: parse: FAIL: t != NULL after MemoryError\n");
                fprintf(stderr, "test_parse: parse: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (t == NULL) {
            fprintf(stderr, "test_parse: parse: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_parse: parse: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            s = ndt_as_string(t, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (s != NULL) {
                ndt_free(s);
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_parse: convert: FAIL: s != NULL after MemoryError\n");
                fprintf(stderr, "test_parse: parse: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (s == NULL) {
            fprintf(stderr, "test_parse: convert: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_parse: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_free(s);
        ndt_decref(t);
        count++;
    }
    fprintf(stderr, "test_parse (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_parse_roundtrip(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    char *s;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = parse_roundtrip_tests; *c != NULL; c++) {
        t = ndt_from_string(*c, ctx);
        if (t == NULL) {
            fprintf(stderr, "test_parse_roundtrip: parse: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_parse_roundtrip: parse: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        s = ndt_as_string(t, ctx);
        if (s == NULL) {
            fprintf(stderr, "test_parse_roundtrip: convert: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_parse_roundtrip: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        if (strcmp(s, *c) != 0) {
            fprintf(stderr, "test_parse_roundtrip: convert: FAIL: input:     \"%s\"\n", *c);
            fprintf(stderr, "test_parse_roundtrip: convert: FAIL: roundtrip: \"%s\"\n", s);
            ndt_free(s);
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_free(s);
        ndt_decref(t);
        count++;
    }
    fprintf(stderr, "test_parse_roundtrip (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_parse_error(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = parse_error_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            t = ndt_from_string(*c, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (t != NULL) {
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_parse_error: FAIL: t != NULL after MemoryError\n");
                fprintf(stderr, "test_parse_error: FAIL: input: %s\n", *c);
                return -1;
            }
        }
        if (t != NULL) {
            fprintf(stderr, "test_parse_error: FAIL: unexpected success: \"%s\"\n", *c);
            fprintf(stderr, "test_parse_error: FAIL: t != NULL after %s: %s\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }
        count++;
    }
    fprintf(stderr, "test_parse_error (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_indent(void)
{
    const indent_testcase_t *tc;
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    char *s;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = parse_tests; *c != NULL; c++) {
        t = ndt_from_string(*c, ctx);
        if (t == NULL) {
            fprintf(stderr, "test_indent: parse: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_indent: parse: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            s = ndt_indent(t, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (s != NULL) {
                ndt_free(s);
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_indent: convert: FAIL: s != NULL after MemoryError\n");
                fprintf(stderr, "test_indent: convert: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (s == NULL) {
            fprintf(stderr, "test_indent: convert: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_indent: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_free(s);
        ndt_decref(t);
        count++;
    }

    for (tc = indent_tests; tc->input != NULL; tc++) {
        t = ndt_from_string(tc->input, ctx);
        if (t == NULL) {
            fprintf(stderr, "test_indent: parse: FAIL: expected success: \"%s\"\n", tc->input);
            fprintf(stderr, "test_indent: parse: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            s = ndt_indent(t, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (s != NULL) {
                ndt_free(s);
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_indent: convert: FAIL: s != NULL after MemoryError\n");
                fprintf(stderr, "test_indent: convert: FAIL: %s\n", tc->input);
                return -1;
            }
        }
        if (s == NULL) {
            fprintf(stderr, "test_indent: convert: FAIL: expected success: \"%s\"\n", tc->input);
            fprintf(stderr, "test_indent: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        if (strcmp(s, tc->indented) != 0) {
            fprintf(stderr, "test_indent: convert: FAIL: expected success: \"%s\"\n", tc->input);
            fprintf(stderr, "test_indent: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_free(s);
        ndt_decref(t);
        count++;
    }

    fprintf(stderr, "test_indent (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_typedef(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = typedef_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            t = ndt_from_string("10 * 20 * {a : int64, b : ref(float64)}", ctx);

            ndt_set_alloc_fail();
            (void)ndt_typedef(*c, t, NULL, ctx);
            ndt_set_alloc();
            ndt_decref(t);

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (ndt_typedef_find(*c, ctx) != NULL) {
                fprintf(stderr, "test_typedef: FAIL: key in map after MemoryError\n");
                fprintf(stderr, "test_typedef: FAIL: input: %s\n", *c);
                ndt_context_del(ctx);
                return -1;
            }
        }

        if (ndt_typedef_find(*c, ctx) == NULL) {
            fprintf(stderr, "test_typedef: FAIL: key not found: \"%s\"\n", *c);
            fprintf(stderr, "test_typedef: FAIL: lookup failed after %s: %s\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        count++;
    }

    fprintf(stderr, "test_typedef (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_typedef_duplicates(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = typedef_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            t = ndt_from_string("10 * 20 * {a : int64, b : ref(float64)}", ctx);

            ndt_set_alloc_fail();
            (void)ndt_typedef(*c, t, NULL, ctx);
            ndt_set_alloc();
            ndt_decref(t);

            if (ctx->err != NDT_MemoryError) {
                if (ndt_typedef_find(*c, ctx) == NULL) {
                    fprintf(stderr, "test_typedef: FAIL: key should be in map\n");
                    fprintf(stderr, "test_typedef: FAIL: input: %s\n", *c);
                    ndt_context_del(ctx);
                    return -1;
                }
                break;
            }
        }

        t = ndt_from_string("10 * 20 * {a : int64, b : ref(float64)}", ctx);
        (void)ndt_typedef(*c, t, NULL, ctx);
        ndt_decref(t);

        if (ctx->err != NDT_ValueError) {
            fprintf(stderr, "test_typedef: FAIL: no value error after duplicate key\n");
            fprintf(stderr, "test_typedef: FAIL: input: %s\n", *c);
            ndt_context_del(ctx);
            return -1;
        }

        count++;
    }

    fprintf(stderr, "test_typedef_duplicates (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_typedef_error(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = typedef_error_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            t = ndt_from_string("10 * 20 * {a : int64, b : ref(float64)}", ctx);

            ndt_set_alloc_fail();
            (void)ndt_typedef(*c, t, NULL, ctx);
            ndt_set_alloc();
            ndt_decref(t);

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (ndt_typedef_find(*c, ctx) != NULL) {
                fprintf(stderr, "test_typedef_error: FAIL: key in map after MemoryError\n");
                fprintf(stderr, "test_typedef: FAIL: input: %s\n", *c);
                ndt_context_del(ctx);
                return -1;
            }
        }

        if (ndt_typedef_find(*c, ctx) != NULL) {
            fprintf(stderr, "test_typedef_error: FAIL: unexpected success: \"%s\"\n", *c);
            fprintf(stderr, "test_typedef_error: FAIL: key in map after %s: %s\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        count++;
    }

    fprintf(stderr, "test_typedef_error (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_equal(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t, *u;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = parse_roundtrip_tests; *c && *(c+1); c++) {
        ndt_err_clear(ctx);

        t = ndt_from_string(*c, ctx);
        if (t == NULL) {
            ndt_context_del(ctx);
            fprintf(stderr, "test_equal: FAIL: could not parse \"%s\"\n", *c);
            return -1;
        }

        u = ndt_from_string(*(c+1), ctx);
        if (u == NULL) {
            ndt_decref(t);
            ndt_context_del(ctx);
            fprintf(stderr, "test_equal: FAIL: could not parse \"%s\"\n", *(c+1));
            return -1;
        }

        if (!ndt_equal(t, t)) {
            ndt_decref(t);
            ndt_decref(u);
            ndt_context_del(ctx);
            fprintf(stderr, "test_equal: FAIL: \"%s\" != \"%s\"\n", *c, *c);
            return -1;
        }

        if (ndt_equal(t, u)) {
            ndt_decref(t);
            ndt_decref(u);
            fprintf(stderr, "test_equal: FAIL: \"%s\" == \"%s\"\n", *c, *(c+1));
            return -1;
        }

        ndt_decref(t);
        ndt_decref(u);
        count++;
    }

    fprintf(stderr, "test_equal (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_match(void)
{
    const match_testcase_t *t;
    ndt_context_t *ctx;
    const ndt_t *p;
    const ndt_t *c;
    int ret, count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (t = match_tests; t->pattern != NULL; t++) {
        p = ndt_from_string(t->pattern, ctx);
        if (p == NULL) {
            fprintf(stderr, "test_match: FAIL: could not parse \"%s\"\n", t->pattern);
            ndt_context_del(ctx);
            return -1;
        }

        c = ndt_from_string(t->candidate, ctx);
        if (c == NULL) {
            ndt_decref(p);
            ndt_context_del(ctx);
            fprintf(stderr, "test_match: FAIL: could not parse \"%s\"\n", t->candidate);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            ret = ndt_match(p, c, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (ret != -1) {
                ndt_decref(p);
                ndt_decref(c);
                ndt_context_del(ctx);
                fprintf(stderr, "test_match: FAIL: expect ret == -1 after MemoryError\n");
                fprintf(stderr, "test_match: FAIL: \"%s\"\n", t->pattern);
                return -1;
            }
        }

        if (ret != t->expected) {
            ndt_decref(p);
            ndt_decref(c);
            ndt_context_del(ctx);
            fprintf(stderr, "test_match: FAIL: expected %s\n", t->expected ? "true" : "false");
            fprintf(stderr, "test_match: FAIL: pattern: \"%s\"\n", t->pattern);
            fprintf(stderr, "test_match: FAIL: candidate: \"%s\"\n", t->candidate);
            return -1;
        }

        ndt_decref(p);
        ndt_decref(c);
        count++;
    }
    fprintf(stderr, "test_match (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_unify(void)
{
    const unify_testcase_t *t;
    ndt_context_t *ctx;
    const ndt_t *t1;
    const ndt_t *t2;
    const ndt_t *expected;
    const ndt_t *ret;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (t = unify_tests; t->t1 != NULL; t++) {
        t1 = ndt_from_string(t->t1, ctx);
        if (t1 == NULL) {
            fprintf(stderr, "test_unify: FAIL: could not parse t1 \"%s\"\n", t->t1);
            ndt_context_del(ctx);
            return -1;
        }

        t2 = ndt_from_string(t->t2, ctx);
        if (t2 == NULL) {
            ndt_decref(t1);
            ndt_context_del(ctx);
            fprintf(stderr, "test_unify: FAIL: could not parse t2 \"%s\"\n", t->t2);
            return -1;
        }

        expected = NULL;
        if (t->expected) {
            expected = ndt_from_string(t->expected, ctx);
            if (expected == NULL) {
                ndt_decref(t1);
                ndt_decref(t2);
                ndt_context_del(ctx);
                fprintf(stderr, "test_unify: FAIL: could not parse expected \"%s\"\n", t->expected);
                return -1;
            }
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            ret = ndt_unify(t1, t2, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (ret != NULL) {
                ndt_decref(t1);
                ndt_decref(t2);
                ndt_decref(expected);
                ndt_context_del(ctx);
                fprintf(stderr, "test_unify: FAIL: expect ret == NULL after MemoryError\n");
                fprintf(stderr, "test_unify: FAIL: \"%s\"\n", t->t1);
                return -1;
            }
        }

        ndt_decref(t1);
        ndt_decref(t2);

        if (ret == NULL && expected == NULL) {
            ndt_err_clear(ctx);
            continue;
        }

        if ((ret != NULL && expected != NULL) && ndt_equal(ret, expected)) {
            ndt_decref(ret);
            ndt_decref(expected);
            count++;
            continue;
        }

        ndt_decref(expected);
        ndt_decref(ret);
        fprintf(stderr, "test_unify: FAIL: expected \"%s\"\n", t->expected);
        fprintf(stderr, "test_unify: FAIL: t1:      \"%s\"\n", t->t1);
        fprintf(stderr, "test_unify: FAIL: t2:      \"%s\"\n", t->t2);
        return -1;
    }
    fprintf(stderr, "test_unify (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

typedef struct {
    int64_t size;
    const ndt_t *types[NDT_MAX_ARGS];
} type_array_t;

static type_array_t
types_from_string(const char * const *s, ndt_context_t *ctx)
{
    type_array_t t;
    int i;

    t.size = -1;

    for (i = 0; s[i] != NULL; i++) {
        t.types[i] = ndt_from_string(s[i], ctx);
        if (t.types[i] == NULL) {
            ndt_type_array_clear(t.types, i);
            return t;
        }
    }

    t.size = i;
    return t;
}

static int
validate_typecheck_test(ndt_apply_spec_t *spec,
                        const ndt_t *sig,
                        const typecheck_testcase_t *test,
                        int ret,
                        ndt_context_t *ctx)
{
    type_array_t expected;
    int64_t i;

    if (!test->success) {
        if (ret == -1) {
            return 0;
        }
        ndt_err_format(ctx, NDT_RuntimeError,
            "test_typecheck: %s: expected success=false, got success=true",
            test->loc);
        return -1;
    }

    expected = types_from_string(test->types, ctx);
    if (expected.size < 0) {
        return -1;
    }

    if (spec->outer_dims != test->outer_dims) {
        ndt_err_format(ctx, NDT_RuntimeError,
            "test_typecheck: %s: expected outer_dims=%d, got outer_dims=%d",
             test->loc, test->outer_dims, spec->outer_dims);
        ndt_type_array_clear(expected.types, expected.size);
        return -1;
    }

    if (spec->nin != sig->Function.nin || spec->nin != test->nin) {
        ndt_err_format(ctx, NDT_RuntimeError,
            "test_typecheck: %s: expected nin=%d, got nin=%d",
             test->loc, test->nin, spec->nin);
        ndt_type_array_clear(expected.types, expected.size);
        return -1;
    }

    if (spec->nout != sig->Function.nout || spec->nout != test->nout) {
        ndt_err_format(ctx, NDT_RuntimeError,
            "test_typecheck: %s: expected nout=%d, got nout=%d",
             test->loc, test->nout, spec->nout);
        ndt_type_array_clear(expected.types, expected.size);
        return -1;
    }

    if (spec->nargs != sig->Function.nargs || spec->nargs != test->nargs ||
        spec->nargs != expected.size) {
        ndt_err_format(ctx, NDT_RuntimeError,
            "test_typecheck: %s: expected nargs=%d, got nargs=%d",
             test->loc, test->nargs, spec->nargs);
        ndt_type_array_clear(expected.types, expected.size);
        return -1;
    }

    for (i = 0; i < spec->nargs; i++) {
        if (!ndt_equal(spec->types[i], expected.types[i])) {
            ndt_err_format(ctx, NDT_RuntimeError,
                "test_typecheck: %s: expected %s, got %s",
                test->loc,
                ndt_ast_repr(expected.types[i], ctx),
                ndt_ast_repr(spec->types[i], ctx));
            ndt_type_array_clear(expected.types, expected.size);
            return -1;
        }
    }

    ndt_type_array_clear(expected.types, expected.size);

    return 0;
}

static int
test_typecheck(void)
{
    NDT_STATIC_CONTEXT(ctx);
    ndt_apply_spec_t spec = ndt_apply_spec_empty;
    const typecheck_testcase_t *test;
    const ndt_t *types[NDT_MAX_DIM] = {NULL};
    const int64_t li[NDT_MAX_DIM] = {0};
    type_array_t args;
    type_array_t kwargs;
    const ndt_t *sig = NULL;
    int count = 0;
    int ret = -1;

    for (test = typecheck_tests; test->signature != NULL; test++) {
        sig = ndt_from_string(test->signature, &ctx);
        if (sig == NULL) {
            ndt_err_format(&ctx, NDT_RuntimeError,
                "test_typecheck: could not parse \"%s\"\n", test->signature);
            goto error;
        }

        args = types_from_string(test->args, &ctx);
        if (args.size < 0) {
            goto error;
        }
        const int nin = (int)args.size;

        kwargs = types_from_string(test->kwargs, &ctx);
        if (kwargs.size < 0) {
            goto error;
        }
        const int nout = (int)kwargs.size;

        for (int i = 0; i < nin; i++) {
            types[i] = args.types[i];
        }

        for (int i = 0; i < nout; i++) {
            types[nin+i] = kwargs.types[i];
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(&ctx);

            ndt_set_alloc_fail();
            ret = ndt_typecheck(&spec, sig, types, li, nin, nout, false, NULL,
                                NULL, &ctx);
            ndt_set_alloc();

            if (ctx.err != NDT_MemoryError) {
                break;
            }

            assert(spec.flags == 0);
            assert(spec.outer_dims == 0);
            assert(spec.nin == 0);
            assert(spec.nout == 0);
            assert(spec.nargs == 0);

            if (ret != -1) {
                ndt_err_format(&ctx, NDT_RuntimeError,
                    "test_typecheck: %s: nout != -1 after MemoryError\n",
                    test->loc);
                goto error;
            }
        }

        ndt_type_array_clear(args.types, args.size);
        ndt_type_array_clear(kwargs.types, kwargs.size);
        ndt_err_clear(&ctx);

        ret = validate_typecheck_test(&spec, sig, test, ret, &ctx);

        ndt_apply_spec_clear(&spec);
        ndt_decref(sig);

        if (ret < 0) {
            goto error;
        }

        count++;
    }

    ret = 0;
    fprintf(stderr, "test_typecheck (%d test cases)\n", count);


out:
    ndt_context_del(&ctx);
    return ret;

error:
   ret = -1;
   ndt_err_fprint(stderr, &ctx);
   goto out;
}

static int
test_numba(void)
{
    NDT_STATIC_CONTEXT(ctx);
    const numba_testcase_t *test;
    const ndt_t *t;
    char *sig = NULL;
    char *core = NULL;
    int count = 0;
    int ret = -1;

    for (test = numba_tests; test->signature != NULL; test++) {
        t = ndt_from_string(test->signature, &ctx);
        if (t == NULL) {
            ndt_err_format(&ctx, NDT_RuntimeError,
                "test_numba: could not parse \"%s\"\n", test->signature);
            goto error;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(&ctx);

            ndt_set_alloc_fail();
            ret = ndt_to_nbformat(&sig, &core, t, &ctx);
            ndt_set_alloc();

            if (ctx.err != NDT_MemoryError) {
                break;
            }

            if (ret != -1) {
                ndt_decref(t);
                ndt_err_format(&ctx, NDT_RuntimeError,
                    "test_numba: expect ret == -1 after MemoryError\n"
                    "test_numba: \"%s\"\n", test->signature);
                goto error;
            }
        }

        ndt_err_clear(&ctx);
        ndt_decref(t);

        if (sig == NULL || strcmp(sig, test->sig) != 0) {
            ndt_err_format(&ctx, NDT_RuntimeError,
                "test_numba: input: \"%s\" output: \"%s\"\n", test->sig, sig);
            goto error;
        }

        if (core == NULL || strcmp(core, test->core) != 0) {
            ndt_err_format(&ctx, NDT_RuntimeError,
                "test_numba: input: \"%s\" output: \"%s\"\n", test->core, core);
            goto error;
        }

        ndt_free(sig);
        ndt_free(core);
        sig = core = NULL;

        count++;
    }

    ret = 0;
    fprintf(stderr, "test_numba (%d test cases)\n", count);


out:
    ndt_context_del(&ctx);
    return ret;

error:
   ret = -1;
   ndt_err_fprint(stderr, &ctx);
   goto out;
}

static int
test_static_context(void)
{
    const char **c;
    NDT_STATIC_CONTEXT(ctx);
    const ndt_t *t;
    char *s;
    int count = 0;

    for (c = parse_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(&ctx);

            ndt_set_alloc_fail();
            t = ndt_from_string(*c, &ctx);
            ndt_set_alloc();

            if (ctx.err != NDT_MemoryError) {
                break;
            }

            if (t != NULL) {
                ndt_decref(t);
                fprintf(stderr, "test_static_context: FAIL: t != NULL after MemoryError\n");
                fprintf(stderr, "test_static_context: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (t == NULL) {
            fprintf(stderr, "test_static_context: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_static_context: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx.err),
                    ndt_context_msg(&ctx));
            ndt_context_del(&ctx);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(&ctx);

            ndt_set_alloc_fail();
            s = ndt_as_string(t, &ctx);
            ndt_set_alloc();

            if (ctx.err != NDT_MemoryError) {
                break;
            }

            if (s != NULL) {
                ndt_free(s);
                ndt_decref(t);
                fprintf(stderr, "test_static_context: FAIL: s != NULL after MemoryError\n");
                fprintf(stderr, "test_static_context: FAIL: %s\n", *c);
                ndt_context_del(&ctx);
                return -1;
            }
        }
        if (s == NULL) {
            fprintf(stderr, "test_static_context: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_static_context: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx.err),
                    ndt_context_msg(&ctx));
            ndt_decref(t);
            ndt_context_del(&ctx);
            return -1;
        }

        ndt_free(s);
        ndt_decref(t);
        count++;
    }

    s = "2 * 1000000000000000000000000000 * complex128";
    t = ndt_from_string(s, &ctx);
    if (s == NULL) {
        fprintf(stderr, "test_static_context: FAIL: expected failure: \"%s\"\n", s);
        ndt_decref(t);
        ndt_context_del(&ctx);
        return -1;
    }
    count++;

    ndt_context_del(&ctx);
    fprintf(stderr, "test_static_context (%d test cases)\n", count);

    return 0;
}

typedef struct {
    const char *str;
    ndt_ssize_t hash;
} hash_testcase_t;

static int
cmp_hash_testcase(const void *x, const void *y)
{
    const hash_testcase_t *p = (const hash_testcase_t *)x;
    const hash_testcase_t *q = (const hash_testcase_t *)y;

    if (p->hash < q->hash) {
        return -1;
    }
    else if (p->hash == q->hash) {
        return 0;
    }

    return 1;
}

static int
test_hash(void)
{
    NDT_STATIC_CONTEXT(ctx);
    hash_testcase_t buf[1000];
    ptrdiff_t n = 1;
    const char **c;
    const ndt_t *t;
    hash_testcase_t x;
    int i;

    for (c = parse_roundtrip_tests; *c != NULL; c++) {
        n = c - parse_roundtrip_tests;
        if (n >= 1000) {
            break;
        }

        ndt_err_clear(&ctx);

        t = ndt_from_string(*c, &ctx);
        if (t == NULL) {
            fprintf(stderr, "test_hash: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_hash: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx.err),
                    ndt_context_msg(&ctx));
            ndt_context_del(&ctx);
            return -1;
        }

        x.hash = ndt_hash(t, &ctx);
        x.str = *c;
        ndt_decref(t);

        if (x.hash == -1) {
            fprintf(stderr, "test_hash: FAIL: hash==-1\n\n");
            ndt_context_del(&ctx);
            return -1;
        }

        buf[n] = x;
    }

    qsort(buf, n, sizeof *buf, cmp_hash_testcase);
    for (i = 0; i < n-1; i++) {
        if (buf[i].hash == buf[i+1].hash) {
            fprintf(stderr,
                "test_hash: duplicate hash for %s: %" PRI_ndt_ssize "\n\n",
                buf[i].str, buf[i].hash);
        }
    }

    t = ndt_from_string("var * {a: float64, b: string}", &ctx);
    if (t == NULL) {
        fprintf(stderr, "test_hash: FAIL: expected success\n\n");
        ndt_context_del(&ctx);
        return -1;
    }

#ifdef TEST_ALLOC
    alloc_fail = 1;
    ndt_set_alloc_fail();
    x.hash = ndt_hash(t, &ctx);
    ndt_set_alloc();

    ndt_decref(t);

    if (x.hash != -1 || ctx.err != NDT_MemoryError) {
        fprintf(stderr, "test_hash: FAIL: expected failure, got %" PRI_ndt_ssize "\n\n", x.hash);
        ndt_context_del(&ctx);
        return -1;
    }
#endif

    ndt_context_del(&ctx);
    fprintf(stderr, "test_hash (%d test cases)\n", (int)n);

    return 0;
}

static int
test_copy(void)
{
    NDT_STATIC_CONTEXT(ctx);
    const char **c;
    const ndt_t *t, *u;
    int count = 0;

    for (c = parse_tests; *c != NULL; c++) {
        t = ndt_from_string(*c, &ctx);
        if (t == NULL) {
            fprintf(stderr, "test_copy: FAIL: from_string: \"%s\"\n", *c);
            fprintf(stderr, "test_copy: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx.err),
                    ndt_context_msg(&ctx));
            ndt_context_del(&ctx);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(&ctx);

            ndt_set_alloc_fail();
            u = ndt_copy(t, &ctx);
            ndt_set_alloc();

            if (ctx.err != NDT_MemoryError) {
                break;
            }

            if (u != NULL) {
                fprintf(stderr, "test_copy: FAIL: unexpected success: \"%s\"\n", *c);
                ndt_decref(u);
                ndt_decref(t);
                ndt_context_del(&ctx);
                return -1;
            }
        }
        if (u == NULL) {
            fprintf(stderr, "test_copy: FAIL: copying failed: \"%s\"\n", *c);
            fprintf(stderr, "test_copy: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx.err),
                    ndt_context_msg(&ctx));
            ndt_decref(t);
            ndt_context_del(&ctx);
            return -1;
        }

        if (!ndt_equal(t, u)) {
            fprintf(stderr, "test_copy: FAIL: copy: not equal\n\n");
            ndt_decref(t);
            ndt_decref(u);
            ndt_context_del(&ctx);
            return -1;
        }

        ndt_decref(u);
        ndt_decref(t);
        count++;
    }

    ndt_context_del(&ctx);
    fprintf(stderr, "test_copy (%d test cases)\n", count);

    return 0;
}

static int
test_buffer(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = buffer_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            t = ndt_from_bpformat(*c, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (t != NULL) {
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_buffer: convert: FAIL: t != NULL after MemoryError\n");
                fprintf(stderr, "test_buffer: convert: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (t == NULL) {
            fprintf(stderr, "test_buffer: convert: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_buffer: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        ndt_decref(t);
        count++;
    }
    fprintf(stderr, "test_buffer (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_buffer_roundtrip(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    char *s;
    int count = 0;
    int ret;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = buffer_roundtrip_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            t = ndt_from_bpformat(*c, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (t != NULL) {
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_buffer: convert: FAIL: t != NULL after MemoryError\n");
                fprintf(stderr, "test_buffer: convert: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (t == NULL) {
            fprintf(stderr, "test_buffer: convert: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_buffer: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_context_del(ctx);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            s = ndt_to_bpformat(t, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (s != NULL) {
                ndt_free(s);
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_buffer_roundtrip: convert: FAIL: s != NULL after MemoryError\n");
                fprintf(stderr, "test_buffer_roundtrip: convert: FAIL: %s\n", *c);
                return -1;
            }
        }
        if (s == NULL) {
            fprintf(stderr, "test_buffer_roundtrip: convert: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_buffer_roundtrip: convert: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }

        ret = strcmp(s, *c);
        ndt_decref(t);

        if (ret != 0) {
            fprintf(stderr, "test_buffer_roundtrip: convert: FAIL: input: \"%s\" output: \"%s\"\n", *c, s);
            ndt_free(s);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_free(s);
        count++;
    }
    fprintf(stderr, "test_buffer_roundtrip (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_buffer_error(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t;
    int count = 0;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = buffer_error_tests; *c != NULL; c++) {
        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            t = ndt_from_bpformat(*c, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (t != NULL) {
                ndt_decref(t);
                ndt_context_del(ctx);
                fprintf(stderr, "test_buffer_error: FAIL: t != NULL after MemoryError\n");
                fprintf(stderr, "test_buffer_error: FAIL: input: %s\n", *c);
                return -1;
            }
        }
        if (t != NULL) {
            fprintf(stderr, "test_buffer_error: FAIL: unexpected success: \"%s\"\n", *c);
            fprintf(stderr, "test_buffer_error: FAIL: t != NULL after %s: %s\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_context_del(ctx);
            return -1;
        }
        count++;
    }
    fprintf(stderr, "test_buffer_error (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

static int
test_serialize(void)
{
    const char **c;
    ndt_context_t *ctx;
    const ndt_t *t, *u;
    int count = 0;
    char *bytes;
    int64_t len;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "error: out of memory");
        return -1;
    }

    for (c = parse_tests; *c != NULL; c++) {
        t = ndt_from_string(*c, ctx);
        if (t == NULL) {
            ndt_context_del(ctx);
            fprintf(stderr,
                "test_serialize: FAIL: unexpected failure in from_string");
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);
            bytes = NULL;

            ndt_set_alloc_fail();
            len = ndt_serialize(&bytes, t, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (len >= 0 || bytes != NULL) {
                ndt_decref(t);
                ndt_free(bytes);
                ndt_context_del(ctx);
                fprintf(stderr, "test_serialize: FAIL: invalid len or bytes after MemoryError\n");
                fprintf(stderr, "test_serialize: FAIL: %s\n", *c);
                return -1;
            }
        }

        if (len < 0 || bytes == NULL) {
            ndt_decref(t);
            ndt_free(bytes);
            ndt_context_del(ctx);
            fprintf(stderr, "test_serialize: FAIL: invalid len or bytes (expected success)\n");
            fprintf(stderr, "test_serialize: FAIL: %s\n", *c);
            return -1;
        }

        for (alloc_fail = 1; alloc_fail < INT_MAX; alloc_fail++) {
            ndt_err_clear(ctx);

            ndt_set_alloc_fail();
            u = ndt_deserialize(bytes, len, ctx);
            ndt_set_alloc();

            if (ctx->err != NDT_MemoryError) {
                break;
            }

            if (u != NULL) {
                ndt_decref(t);
                ndt_decref(u);
                ndt_free(bytes);
                ndt_context_del(ctx);
                fprintf(stderr, "test_serialize: FAIL: u != NULL after MemoryError\n");
                fprintf(stderr, "test_serialize: FAIL: %s\n", *c);
                return -1;
            }
        }

        if (u == NULL) {
            fprintf(stderr, "test_serialize: FAIL: expected success: \"%s\"\n", *c);
            fprintf(stderr, "test_serialize: FAIL: got: %s: %s\n\n",
                    ndt_err_as_string(ctx->err),
                    ndt_context_msg(ctx));
            ndt_decref(t);
            ndt_free(bytes);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_free(bytes);

        if (!ndt_equal(u, t)) {
            fprintf(stderr, "test_serialize: FAIL: u != v in %s\n", *c);
            ndt_decref(t);
            ndt_decref(u);
            ndt_context_del(ctx);
            return -1;
        }

        ndt_decref(t);
        ndt_decref(u);

        count++;
    }
    fprintf(stderr, "test_serialize (%d test cases)\n", count);

    ndt_context_del(ctx);
    return 0;
}

#if defined(__linux__)
static int
test_serialize_fuzz(void)
{
    ndt_context_t *ctx;
    char *buf;
    ssize_t ret, n;
    int src;
    int64_t i;

    ctx = ndt_context_new();
    if (ctx == NULL) {
        fprintf(stderr, "\nmalloc error in fuzz tests\n");
        return -1;
    }

    src = open("/dev/urandom", O_RDONLY);
    if (src < 0) {
        ndt_context_del(ctx);
        fprintf(stderr, "\ncould not open /dev/urandom\n");
        return -1;
    }

    for (i = 0; i < 10000; i++) {
        n = rand() % 1000;
        buf = ndt_alloc(n, 1);
        if (buf == NULL) {
            close(src);
            ndt_context_del(ctx);
            fprintf(stderr, "malloc error in fuzz tests\n");
            return -1;
        }

        ret = read(src, buf, n);
        if (ret < 0) {
            ndt_free(buf);
            close(src);
            ndt_context_del(ctx);
            fprintf(stderr, "\nread error in fuzz tests\n");
            return -1;
        }

        ndt_err_clear(ctx);
        const ndt_t *t = ndt_deserialize(buf, n, ctx);
        if (t != NULL) {
            ndt_decref(t);
        }

        ndt_free(buf);
    }
    fprintf(stderr, "test_serialize_fuzz (%" PRIi64 " test cases)\n", i);

    close(src);
    ndt_context_del(ctx);
    return 0;
}
#endif

static int (*tests[])(void) = {
  test_parse,
  test_parse_roundtrip,
  test_parse_error,
  test_indent,
  test_typedef,
  test_typedef_duplicates,
  test_typedef_error,
  test_equal,
  test_match,
  test_unify,
  test_typecheck,
  test_numba,
  test_static_context,
  test_hash,
  test_copy,
  test_buffer,
  test_buffer_roundtrip,
  test_buffer_error,
  test_serialize,
#ifdef __linux__
  test_serialize_fuzz,
#endif
#ifdef __GNUC__
  test_struct_align_pack,
  test_array,
#endif
  NULL
};

int
main(void)
{
    int (**f)(void);
    int success = 0;
    int fail = 0;

    if (init_tests() < 0) {
        return 1;
    }

    for (f = tests; *f != NULL; f++) {
        if ((*f)() < 0)
            fail++;
        else
            success++;
    }

    if (fail) {
        fprintf(stderr, "\nFAIL (failures=%d)\n", fail);
    }
    else {
        fprintf(stderr, "\n%d tests OK.\n", success);
    }

    ndt_finalize();
    return fail ? 1 : 0;
}
