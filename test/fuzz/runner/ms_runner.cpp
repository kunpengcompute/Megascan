#include "fuzz_test.h"

#include <algorithm>
#include <sstream>

extern "C" {
#include "ms.h"
#include "ms_compile.h"
#include "ms_expr.h"
#include "ms_handler.h"
#include "ms_logical.h"
#include "ms_rule.h"
#include "ms_runtime.h"
}

namespace {

class MsRunner final : public Runner {
public:
    bool run(const FuzzTestCase &tc, std::string &error) override {
        stats_.cases++;
        return databaseAc(tc, error) && legacyAc(tc, error) &&
               hashApis(tc, error) && logicalApis(tc, error);
    }

    const FuzzStats &stats() const override { return stats_; }

private:
    void hit(const char *name) { stats_.calls[name]++; }

    bool check(const FuzzTestCase &tc, const char *api, int actual,
               int expected, std::string &error) {
        if (actual == expected) {
            return true;
        }
        std::ostringstream os;
        os << "seed=" << tc.seed << " round=" << tc.round << " api=" << api
           << " expected=" << expected << " actual=" << actual;
        error = os.str();
        return false;
    }

    bool checkScan(const FuzzTestCase &tc, const char *api, int actual,
                   std::string &error) {
        return actual == MS_SUCCESS || actual == MS_FIND_FAIL ||
               check(tc, api, actual, MS_SUCCESS, error);
    }

    static std::vector<const char *> pointers(const FuzzTestCase &tc) {
        std::vector<const char *> result;
        for (const auto &expression : tc.expressions) {
            result.push_back(expression.c_str());
        }
        return result;
    }

    bool databaseAc(const FuzzTestCase &tc, std::string &error) {
        auto expr = pointers(tc);
        std::vector<ms_expr_ext_t> ext(expr.size());
        ms_database_t *empty = nullptr;
        ms_database_t *db = nullptr;
        ms_scratch_t *scratch = nullptr;
        ms_stream_t *stream = nullptr;
        uint32_t ids[64] = {};
        uint32_t count = 0;
        uint32_t eventCount = 0;
        int ret;
        bool ok = false;

        hit("ms_create_database");
        if (!check(tc, "ms_create_database",
                   ms_create_database(&empty, MS_MATCHER_TYPE_AC, 0, 0),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_free_database");
        if (!check(tc, "ms_free_database(empty)", ms_free_database(empty),
                   MS_SUCCESS, error)) {
            empty = nullptr;
            goto cleanup;
        }
        empty = nullptr;

        if ((tc.round & 1u) == 0u) {
            hit("ms_compile_database_multi");
            ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                            expr.data(), tc.flags.data(),
                                            tc.ids.data(), expr.size());
        } else {
            ext.back().flags = MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET;
            ext.back().max_offset = tc.data.size();
            hit("ms_compile_database_multi_ext");
            ret = ms_compile_database_multi_ext(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                                expr.data(), tc.flags.data(),
                                                tc.ids.data(), ext.data(),
                                                expr.size());
        }
        if (!check(tc, "compile database AC", ret, MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_alloc_scratch");
        if (!check(tc, "ms_alloc_scratch", ms_alloc_scratch(db, &scratch),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_scan_database");
        ret = ms_scan_database(db, tc.data.data(), tc.data.size(), scratch,
                               ids, &count, 64u);
        if (!checkScan(tc, "ms_scan_database", ret, error)) {
            goto cleanup;
        }
        hit("ms_get_match_events");
        if (!check(tc, "ms_get_match_events",
                   ms_get_match_events(scratch, nullptr, &eventCount, 0),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }

        hit("ms_open_stream");
        if (!check(tc, "ms_open_stream", ms_open_stream(db, &stream),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        count = 0;
        hit("ms_scan_stream");
        ret = ms_scan_stream(stream, tc.data.data(), tc.data.size(), scratch,
                             ids, &count, 64u);
        if (!checkScan(tc, "ms_scan_stream", ret, error)) {
            goto cleanup;
        }
        count = 0;
        hit("ms_flush_stream");
        ret = ms_flush_stream(stream, scratch, ids, &count, 64u);
        if (!checkScan(tc, "ms_flush_stream", ret, error)) {
            goto cleanup;
        }
        count = 0;
        hit("ms_close_stream");
        ret = ms_close_stream(stream, scratch, ids, &count, 64u);
        stream = nullptr;
        if (!checkScan(tc, "ms_close_stream", ret, error)) {
            goto cleanup;
        }

        {
            ms_rule_t rule = {tc.ids[0],
                              static_cast<uint32_t>(tc.expressions[0].size()),
                              reinterpret_cast<uint8_t *>(
                                  const_cast<char *>(tc.expressions[0].c_str()))};
            hit("ms_find_database_rule");
            ret = ms_find_database_rule(db, &rule);
            if (!checkScan(tc, "ms_find_database_rule", ret, error)) {
                goto cleanup;
            }
            rule.alias = tc.ids[0];
            hit("ms_remove_database_rule");
            if (!check(tc, "ms_remove_database_rule",
                       ms_remove_database_rule(db, &rule, 1u),
                       MS_SUCCESS, error)) {
                goto cleanup;
            }
        }
        ok = true;

cleanup:
        if (stream != nullptr) {
            hit("ms_close_handler_stream");
            (void)ms_close_handler_stream(stream);
        }
        if (scratch != nullptr) {
            hit("ms_free_scratch");
            ret = ms_free_scratch(scratch);
            if (ok && !check(tc, "ms_free_scratch", ret, MS_SUCCESS, error)) {
                ok = false;
            }
        }
        if (db != nullptr) {
            hit("ms_free_database");
            ret = ms_free_database(db);
            if (ok && !check(tc, "ms_free_database", ret, MS_SUCCESS, error)) {
                ok = false;
            }
        }
        if (empty != nullptr) {
            hit("ms_free_database");
            (void)ms_free_database(empty);
        }
        return ok;
    }

    bool legacyAc(const FuzzTestCase &tc, std::string &error) {
        auto expr = pointers(tc);
        std::vector<ms_expr_ext_t> ext(expr.size());
        ms_handler_t handler = {};
        ms_stream_t *stream = nullptr;
        uint32_t ids[64] = {};
        uint32_t combinations[64] = {};
        uint32_t count = 0;
        uint32_t combinationCount = 0;
        int ret;
        bool ok = false;

        hit("ms_handler_init");
        if (!check(tc, "ms_handler_init(AC)",
                   ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0),
                   MS_SUCCESS, error)) {
            return false;
        }
        const char *compileApi;
        if (tc.round % 3u == 0u) {
            compileApi = "ms_compile_multi";
            hit(compileApi);
            ret = ms_compile_multi(&handler, expr.data(), tc.flags.data(),
                                   tc.ids.data(), expr.size());
        } else if (tc.round % 3u == 1u) {
            compileApi = "ms_compile_multi_ext";
            ext.back().flags = MS_EXT_FLAG_MAX_OFFSET;
            ext.back().max_offset = tc.data.size();
            hit(compileApi);
            ret = ms_compile_multi_ext(&handler, expr.data(), tc.flags.data(),
                                       tc.ids.data(), ext.data(), expr.size());
        } else {
            compileApi = "ms_add_exprs";
            hit(compileApi);
            ret = ms_add_exprs(&handler, expr.data(), tc.ids.data(),
                               tc.flags.data(), expr.size());
        }
        if (!check(tc, compileApi, ret, MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_scan_expr");
        ret = ms_scan_expr(&handler, tc.data.data(), tc.data.size(), ids,
                           &count, 64u);
        if (!checkScan(tc, "ms_scan_expr", ret, error)) {
            goto cleanup;
        }
        hit("ms_compile_scan_ac_combinations");
        ret = ms_compile_scan_ac_combinations(&handler, tc.data.data(),
                                              tc.data.size(), combinations,
                                              &combinationCount, 64u);
        if (!checkScan(tc, "ms_compile_scan_ac_combinations", ret, error)) {
            goto cleanup;
        }
        hit("ms_compile_eval_combinations");
        ret = ms_compile_eval_combinations(&handler, ids, count, combinations,
                                           &combinationCount, 64u);
        if (!checkScan(tc, "ms_compile_eval_combinations", ret, error)) {
            goto cleanup;
        }
        hit("ms_open_handler_stream");
        if (!check(tc, "ms_open_handler_stream",
                   ms_open_handler_stream(&handler, &stream),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_scan_rule_stream");
        ret = ms_scan_rule_stream(stream, tc.data.data(), tc.data.size());
        if (!checkScan(tc, "ms_scan_rule_stream", ret, error)) {
            goto cleanup;
        }
        hit("ms_reset_stream");
        if (!check(tc, "ms_reset_stream", ms_reset_stream(stream),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        count = 0;
        hit("ms_scan_expr_stream");
        ret = ms_scan_expr_stream(stream, tc.data.data(), tc.data.size(), ids,
                                  &count, 64u);
        if (!checkScan(tc, "ms_scan_expr_stream", ret, error)) {
            goto cleanup;
        }
        hit("ms_scan_expr_stream_flush");
        ret = ms_scan_expr_stream_flush(stream, ids, &count, 64u);
        if (!checkScan(tc, "ms_scan_expr_stream_flush", ret, error)) {
            goto cleanup;
        }
        hit("ms_close_handler_stream");
        if (!check(tc, "ms_close_handler_stream",
                   ms_close_handler_stream(stream), MS_SUCCESS, error)) {
            goto cleanup;
        }
        stream = nullptr;
        ok = true;

cleanup:
        if (stream != nullptr) {
            hit("ms_close_handler_stream");
            (void)ms_close_handler_stream(stream);
        }
        hit("ms_handler_destroy");
        ret = ms_handler_destroy(&handler);
        if (ok && !check(tc, "ms_handler_destroy(AC)", ret,
                         MS_SUCCESS, error)) {
            ok = false;
        }
        return ok;
    }

    static uint32_t alternateHash(const uint8_t *key, uint32_t length) {
        uint32_t value = UINT32_C(2166136261);
        for (uint32_t i = 0; i < length; i++) {
            value = (value ^ key[i]) * UINT32_C(16777619);
        }
        return value;
    }

    bool hashApis(const FuzzTestCase &tc, std::string &error) {
        ms_handler_t handler = {};
        ms_database_t *db = nullptr;
        std::vector<std::string> values;
        std::vector<const char *> expr;
        std::vector<uint32_t> ids;
        std::vector<ms_rule_t> rules;
        int ret;
        bool ok = false;

        for (size_t i = 0; i + 1u < tc.expressions.size(); i++) {
            values.push_back(tc.expressions[i].substr(0, 16u));
            ids.push_back(tc.ids[i]);
        }
        for (size_t i = 0; i < values.size(); i++) {
            expr.push_back(values[i].c_str());
            rules.push_back(ms_rule_t{
                ids[i], static_cast<uint32_t>(values[i].size()),
                reinterpret_cast<uint8_t *>(const_cast<char *>(values[i].c_str()))
            });
        }

        hit("ms_handler_init");
        if (!check(tc, "ms_handler_init(HASH)",
                   ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 32u, 4u),
                   MS_SUCCESS, error)) {
            return false;
        }
        hit("ms_set_hash_func");
        if (!check(tc, "ms_set_hash_func",
                   ms_set_hash_func(&handler, alternateHash),
                   MS_SUCCESS, error)) {
            goto handlerCleanup;
        }
        hit("ms_add_rule");
        if (!check(tc, "ms_add_rule",
                   ms_add_rule(&handler, rules.data(), rules.size()),
                   MS_SUCCESS, error)) {
            goto handlerCleanup;
        }
        for (auto rule : rules) {
            hit("ms_find_rule");
            if (!check(tc, "ms_find_rule", ms_find_rule(&handler, &rule),
                       MS_SUCCESS, error)) {
                goto handlerCleanup;
            }
        }
        hit("ms_remove_rule");
        if (!check(tc, "ms_remove_rule",
                   ms_remove_rule(&handler, &rules[0], 1u),
                   MS_SUCCESS, error)) {
            goto handlerCleanup;
        }
        ok = true;

handlerCleanup:
        hit("ms_handler_destroy");
        ret = ms_handler_destroy(&handler);
        if (ok && !check(tc, "ms_handler_destroy(HASH)", ret,
                         MS_SUCCESS, error)) {
            ok = false;
        }
        if (!ok) {
            return false;
        }

        hit("ms_compile_database_multi_ext");
        ret = ms_compile_database_multi_ext(&db, MS_MATCHER_TYPE_HASH, 32u, 4u,
                                            expr.data(), nullptr, ids.data(),
                                            nullptr, expr.size());
        if (!check(tc, "compile database HASH", ret, MS_SUCCESS, error)) {
            return false;
        }
        for (auto rule : rules) {
            hit("ms_find_database_rule");
            if (!check(tc, "ms_find_database_rule(HASH)",
                       ms_find_database_rule(db, &rule),
                       MS_SUCCESS, error)) {
                ok = false;
                break;
            }
        }
        hit("ms_free_database");
        ret = ms_free_database(db);
        return check(tc, "ms_free_database(HASH)", ret, MS_SUCCESS, error) && ok;
    }

    bool logicalApis(const FuzzTestCase &tc, std::string &error) {
        static const char *samples[] = {"0|0", "!0", "0&1", "0|1", "!(0&1)"};
        ms_logical_tree_t tree = {};
        uint8_t vector[1] = {};
        uint8_t value = 0;
        uint8_t matched = 0;
        int ret;

        hit("ms_logical_parse");
        if (!check(tc, "ms_logical_parse",
                   ms_logical_parse(samples[tc.round % 5u], &tree),
                   MS_SUCCESS, error)) {
            return false;
        }
        hit("ms_logical_set");
        ret = ms_logical_set(vector, 2u, 0u, tc.round & 1u);
        if (!check(tc, "ms_logical_set", ret, MS_SUCCESS, error)) {
            ms_logical_tree_destroy(&tree);
            return false;
        }
        hit("ms_logical_get");
        ret = ms_logical_get(vector, 2u, 0u, &value);
        if (!check(tc, "ms_logical_get", ret, MS_SUCCESS, error)) {
            ms_logical_tree_destroy(&tree);
            return false;
        }
        hit("ms_logical_eval");
        ret = ms_logical_eval(&tree, vector, 2u, &matched);
        if (!check(tc, "ms_logical_eval", ret, MS_SUCCESS, error)) {
            ms_logical_tree_destroy(&tree);
            return false;
        }
        hit("ms_logical_tree_destroy");
        ms_logical_tree_destroy(&tree);

        /* The inplace API consumes a direct-operation tree. Parser output can
         * contain MS_LOGICAL_OP_REF nodes and is intended for ms_logical_eval. */
        ms_logical_op_t directOp = {};
        directOp.id = 1u;
        directOp.op = tc.round % 3u;
        directOp.left = 0u;
        directOp.right = 0u;
        ms_logical_tree_t directTree = {};
        directTree.ops = &directOp;
        directTree.op_count = 1u;
        directTree.op_capacity = 1u;
        directTree.result = directOp.id;

        hit("ms_logical_eval_inplace");
        ret = ms_logical_eval_inplace(&directTree, vector, 2u, &matched);
        if (!check(tc, "ms_logical_eval_inplace", ret, MS_SUCCESS, error)) {
            return false;
        }
        hit("ms_logical_clear");
        ret = ms_logical_clear(vector, 2u);
        return check(tc, "ms_logical_clear", ret, MS_SUCCESS, error);
    }

    FuzzStats stats_;
};

} // namespace

std::unique_ptr<Runner> createRunner() {
    return std::unique_ptr<Runner>(new MsRunner());
}
