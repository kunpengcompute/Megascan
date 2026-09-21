#include "fuzz_test.h"

#include <algorithm>
#include <sstream>

extern "C" {
#include "ms.h"
#include "ms_allocator.h"
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
        targetApi_ = tc.targetApi;
        bool ok = true;
        std::string groupError;

        if (!publicApiNegativeVariants(tc, groupError)) {
            error = groupError;
            ok = false;
        }
        groupError.clear();

        if (((!targetApi_.empty() && shouldRun(Group::Allocator)) ||
             tc.exerciseAllocatorApis) &&
            !allocatorApis(tc, groupError)) {
            error = groupError;
            ok = false;
        }
        groupError.clear();
        if (shouldRun(Group::Database) && !databaseAc(tc, groupError)) {
            if (error.empty()) {
                error = groupError;
            }
            ok = false;
        }
        groupError.clear();
        if (shouldRun(Group::Legacy) && !legacyAc(tc, groupError)) {
            if (error.empty()) {
                error = groupError;
            }
            ok = false;
        }
        groupError.clear();
        if (shouldRun(Group::Hash) && !hashApis(tc, groupError)) {
            if (error.empty()) {
                error = groupError;
            }
            ok = false;
        }
        groupError.clear();
        if (shouldRun(Group::Hash) &&
            (targetApi_.empty() || targetApi_ == "ms_add_rule" ||
             targetApi_ == "ms_find_rule" ||
             targetApi_ == "ms_remove_rule") &&
            !acRuleApis(tc, groupError)) {
            if (error.empty()) {
                error = groupError;
            }
            ok = false;
        }
        groupError.clear();
        if (shouldRun(Group::Logical) && !logicalApis(tc, groupError)) {
            if (error.empty()) {
                error = groupError;
            }
            ok = false;
        }
        return ok;
    }

    const FuzzStats &stats() const override { return stats_; }

private:
    enum class Group { Allocator, Database, Legacy, Hash, Logical };

    Group targetGroup() const {
        if (targetApi_.find("ms_set_") == 0 &&
            targetApi_ != "ms_set_hash_func") {
            return Group::Allocator;
        }
        if (targetApi_.find("ms_logical_") == 0) {
            return Group::Logical;
        }
        if (targetApi_ == "ms_set_hash_func" || targetApi_ == "ms_add_rule" ||
            targetApi_ == "ms_find_rule" || targetApi_ == "ms_remove_rule") {
            return Group::Hash;
        }
        if (targetApi_ == "ms_handler_init" ||
            targetApi_ == "ms_handler_destroy" ||
            targetApi_ == "ms_compile_multi" ||
            targetApi_ == "ms_compile_multi_ext" ||
            targetApi_ == "ms_add_exprs" ||
            targetApi_ == "ms_scan_expr" ||
            targetApi_ == "ms_compile_scan_ac_combinations" ||
            targetApi_ == "ms_compile_eval_combinations" ||
            targetApi_ == "ms_open_handler_stream" ||
            targetApi_ == "ms_scan_rule_stream" ||
            targetApi_ == "ms_reset_stream" ||
            targetApi_ == "ms_scan_expr_stream" ||
            targetApi_ == "ms_scan_expr_stream_flush" ||
            targetApi_ == "ms_close_handler_stream") {
            return Group::Legacy;
        }
        return Group::Database;
    }

    bool shouldRun(Group group) const {
        return targetApi_.empty() || targetGroup() == group;
    }

    void hit(const char *name) {
        stats_.invocations[name]++;
        if (targetApi_.empty() || targetApi_ == name) {
            stats_.calls[name]++;
        }
    }

    bool check(const FuzzTestCase &tc, const char *api, int actual,
               int expected, std::string &error) {
        if (actual == expected) {
            return true;
        }
        stats_.failures[api]++;
        std::ostringstream os;
        os << "seed=" << tc.seed << " round=" << tc.round << " api=" << api
           << " expected=" << expected << " actual=" << actual;
        if (error.empty()) {
            error = os.str();
        }
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

    bool isTarget(const char *api) const {
        return targetApi_.empty() || targetApi_ == api;
    }

    bool publicApiNegativeVariants(const FuzzTestCase &tc,
                                   std::string &error) {
        bool ok = true;
        uint32_t ids[4] = {};
        uint32_t count = 0;
        uint8_t data[1] = {0};
        uint8_t logical[1] = {};
        uint8_t value = 0;
        uint8_t matched = 0;
        ms_logical_tree_t tree = {};
        ms_rule_t rule = {};
        const char *exprs[] = {"a"};
        uint32_t exprIds[] = {1u};
        uint32_t flags[] = {0u};

        if (isTarget("ms_set_allocator")) {
            hit("ms_set_allocator");
            if (!check(tc, "ms_set_allocator(default)",
                       ms_set_allocator(nullptr, nullptr, nullptr),
                       MS_SUCCESS, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_set_database_allocator")) {
            hit("ms_set_database_allocator");
            if (!check(tc, "ms_set_database_allocator(default)",
                       ms_set_database_allocator(nullptr, nullptr, nullptr),
                       MS_SUCCESS, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_set_scratch_allocator")) {
            hit("ms_set_scratch_allocator");
            if (!check(tc, "ms_set_scratch_allocator(default)",
                       ms_set_scratch_allocator(nullptr, nullptr, nullptr),
                       MS_SUCCESS, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_set_stream_allocator")) {
            hit("ms_set_stream_allocator");
            if (!check(tc, "ms_set_stream_allocator(default)",
                       ms_set_stream_allocator(nullptr, nullptr, nullptr),
                       MS_SUCCESS, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_set_misc_allocator")) {
            hit("ms_set_misc_allocator");
            if (!check(tc, "ms_set_misc_allocator(default)",
                       ms_set_misc_allocator(nullptr, nullptr, nullptr),
                       MS_SUCCESS, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_handler_init")) {
            ms_handler_t handler = {};
            hit("ms_handler_init");
            if (!check(tc, "ms_handler_init(NULL)",
                       ms_handler_init(nullptr, MS_MATCHER_TYPE_AC, 0, 0),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }

            hit("ms_handler_init");
            if (!check(tc, "ms_handler_init(invalid type)",
                       ms_handler_init(&handler, MS_MATCHER_TYPE_MAX, 0, 0),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }

            hit("ms_handler_init");
            if (check(tc, "ms_handler_init(reinit setup)",
                      ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0),
                      MS_SUCCESS, error)) {
                hit("ms_handler_init");
                if (!check(tc, "ms_handler_init(already built)",
                           ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0),
                           MS_BUILT_ALREADY, error)) {
                    ok = false;
                }
                hit("ms_handler_destroy");
                if (!check(tc, "ms_handler_destroy(reinit cleanup)",
                           ms_handler_destroy(&handler), MS_SUCCESS, error)) {
                    ok = false;
                }
            } else {
                ok = false;
            }
        }

        if (isTarget("ms_handler_destroy")) {
            ms_handler_t handler = {};
            hit("ms_handler_destroy");
            if (!check(tc, "ms_handler_destroy(NULL)",
                       ms_handler_destroy(nullptr), MS_INVALID_PARA, error)) {
                ok = false;
            }

            hit("ms_handler_init");
            if (check(tc, "ms_handler_destroy(invalid type setup)",
                      ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0),
                      MS_SUCCESS, error)) {
                handler.type = MS_MATCHER_TYPE_MAX;
                hit("ms_handler_destroy");
                if (!check(tc, "ms_handler_destroy(invalid type)",
                           ms_handler_destroy(&handler), MS_INVALID_PARA,
                           error)) {
                    ok = false;
                }
                handler.type = MS_MATCHER_TYPE_AC;
                hit("ms_handler_destroy");
                if (!check(tc, "ms_handler_destroy(invalid type cleanup)",
                           ms_handler_destroy(&handler), MS_SUCCESS, error)) {
                    ok = false;
                }
            } else {
                ok = false;
            }
        }

        if (isTarget("ms_compile_multi")) {
            hit("ms_compile_multi");
            if (!check(tc, "ms_compile_multi(NULL handler)",
                       ms_compile_multi(nullptr, exprs, flags, exprIds, 1u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_compile_multi_ext")) {
            hit("ms_compile_multi_ext");
            if (!check(tc, "ms_compile_multi_ext(NULL handler)",
                       ms_compile_multi_ext(nullptr, exprs, flags, exprIds,
                                            nullptr, 1u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_add_exprs")) {
            hit("ms_add_exprs");
            if (!check(tc, "ms_add_exprs(NULL handler)",
                       ms_add_exprs(nullptr, exprs, exprIds, flags, 1u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_create_database")) {
            ms_database_t *db = nullptr;
            ms_database_t *already = reinterpret_cast<ms_database_t *>(1);
            hit("ms_create_database");
            if (!check(tc, "ms_create_database(NULL out)",
                       ms_create_database(nullptr, MS_MATCHER_TYPE_AC, 0, 0),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_create_database");
            if (!check(tc, "ms_create_database(already set)",
                       ms_create_database(&already, MS_MATCHER_TYPE_AC, 0, 0),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_create_database");
            if (!check(tc, "ms_create_database(invalid type)",
                       ms_create_database(&db, MS_MATCHER_TYPE_MAX, 0, 0),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_compile_database_multi") ||
            isTarget("ms_compile_database_multi_ext")) {
            ms_database_t *db = nullptr;
            ms_database_t *already = reinterpret_cast<ms_database_t *>(1);
            if (targetApi_.empty() || targetApi_ == "ms_compile_database_multi") {
                hit("ms_compile_database_multi");
                if (!check(tc, "ms_compile_database_multi(NULL out)",
                           ms_compile_database_multi(nullptr,
                                                     MS_MATCHER_TYPE_AC, 0, 0,
                                                     exprs, flags, exprIds, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_compile_database_multi");
                if (!check(tc, "ms_compile_database_multi(already set)",
                           ms_compile_database_multi(&already,
                                                     MS_MATCHER_TYPE_AC, 0, 0,
                                                     exprs, flags, exprIds, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_compile_database_multi");
                if (!check(tc, "ms_compile_database_multi(invalid type)",
                           ms_compile_database_multi(&db, MS_MATCHER_TYPE_MAX,
                                                     0, 0, exprs, flags,
                                                     exprIds, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
            if (targetApi_.empty() ||
                targetApi_ == "ms_compile_database_multi_ext") {
                hit("ms_compile_database_multi_ext");
                if (!check(tc, "ms_compile_database_multi_ext(NULL out)",
                           ms_compile_database_multi_ext(
                               nullptr, MS_MATCHER_TYPE_AC, 0, 0, exprs,
                               flags, exprIds, nullptr, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_compile_database_multi_ext");
                if (!check(tc, "ms_compile_database_multi_ext(already set)",
                           ms_compile_database_multi_ext(
                               &already, MS_MATCHER_TYPE_AC, 0, 0, exprs,
                               flags, exprIds, nullptr, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
        }

        if (isTarget("ms_free_database")) {
            hit("ms_free_database");
            if (!check(tc, "ms_free_database(NULL)", ms_free_database(nullptr),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_alloc_scratch")) {
            ms_scratch_t *scratch = nullptr;
            ms_scratch_t *already = reinterpret_cast<ms_scratch_t *>(1);
            hit("ms_alloc_scratch");
            if (!check(tc, "ms_alloc_scratch(NULL db)",
                       ms_alloc_scratch(nullptr, &scratch), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_alloc_scratch");
            if (!check(tc, "ms_alloc_scratch(NULL out)",
                       ms_alloc_scratch(nullptr, nullptr), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_alloc_scratch");
            if (!check(tc, "ms_alloc_scratch(already set)",
                       ms_alloc_scratch(nullptr, &already), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
        }

        if (isTarget("ms_free_scratch")) {
            hit("ms_free_scratch");
            if (!check(tc, "ms_free_scratch(NULL)", ms_free_scratch(nullptr),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_scan_database")) {
            hit("ms_scan_database");
            if (!check(tc, "ms_scan_database(NULL db)",
                       ms_scan_database(nullptr, data, 1u, nullptr, ids,
                                        &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_scan_database");
            if (!check(tc, "ms_scan_database(NULL data)",
                       ms_scan_database(nullptr, nullptr, 1u, nullptr, ids,
                                        &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_scan_database");
            if (!check(tc, "ms_scan_database(NULL ids with capacity)",
                       ms_scan_database(nullptr, data, 1u, nullptr, nullptr,
                                        &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_get_match_events")) {
            hit("ms_get_match_events");
            if (!check(tc, "ms_get_match_events(NULL scratch)",
                       ms_get_match_events(nullptr, nullptr, &count, 0u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_get_match_events");
            if (!check(tc, "ms_get_match_events(NULL count)",
                       ms_get_match_events(nullptr, nullptr, nullptr, 0u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_find_database_rule")) {
            hit("ms_find_database_rule");
            if (!check(tc, "ms_find_database_rule(NULL db)",
                       ms_find_database_rule(nullptr, &rule), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_find_database_rule");
            if (!check(tc, "ms_find_database_rule(NULL rule)",
                       ms_find_database_rule(nullptr, nullptr),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_remove_database_rule")) {
            hit("ms_remove_database_rule");
            if (!check(tc, "ms_remove_database_rule(NULL db)",
                       ms_remove_database_rule(nullptr, &rule, 1u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_remove_database_rule");
            if (!check(tc, "ms_remove_database_rule(NULL rule)",
                       ms_remove_database_rule(nullptr, nullptr, 1u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_remove_database_rule");
            if (!check(tc, "ms_remove_database_rule(zero count)",
                       ms_remove_database_rule(nullptr, &rule, 0u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_add_rule") || isTarget("ms_find_rule") ||
            isTarget("ms_remove_rule") || isTarget("ms_set_hash_func")) {
            ms_handler_t handler = {};
            rule.key = data;
            rule.key_len = 1u;
            rule.alias = 1u;

            if (isTarget("ms_add_rule")) {
                hit("ms_add_rule");
                if (!check(tc, "ms_add_rule(NULL handler)",
                           ms_add_rule(nullptr, &rule, 1u), MS_INVALID_PARA,
                           error)) {
                    ok = false;
                }
            }

            if (isTarget("ms_find_rule")) {
                hit("ms_find_rule");
                if (!check(tc, "ms_find_rule(NULL handler)",
                           ms_find_rule(nullptr, &rule), MS_INVALID_PARA,
                           error)) {
                    ok = false;
                }
                hit("ms_find_rule");
                if (!check(tc, "ms_find_rule(NULL rule)",
                           ms_find_rule(&handler, nullptr), MS_INVALID_PARA,
                           error)) {
                    ok = false;
                }
                rule.key = nullptr;
                hit("ms_find_rule");
                if (!check(tc, "ms_find_rule(NULL key)",
                           ms_find_rule(&handler, &rule), MS_INVALID_PARA,
                           error)) {
                    ok = false;
                }
                rule.key = data;
            }

            if (isTarget("ms_remove_rule")) {
                hit("ms_remove_rule");
                if (!check(tc, "ms_remove_rule(NULL handler)",
                           ms_remove_rule(nullptr, &rule, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_remove_rule");
                if (!check(tc, "ms_remove_rule(NULL rule)",
                           ms_remove_rule(&handler, nullptr, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }

            if (isTarget("ms_set_hash_func")) {
                hit("ms_set_hash_func");
                if (!check(tc, "ms_set_hash_func(NULL handler)",
                           ms_set_hash_func(nullptr, alternateHash),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_set_hash_func");
                if (!check(tc, "ms_set_hash_func(NULL func)",
                           ms_set_hash_func(&handler, nullptr),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
        }

        if (isTarget("ms_open_handler_stream")) {
            ms_stream_t *stream = nullptr;
            ms_stream_t *already = reinterpret_cast<ms_stream_t *>(1);
            hit("ms_open_handler_stream");
            if (!check(tc, "ms_open_handler_stream(NULL handler)",
                       ms_open_handler_stream(nullptr, &stream),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_open_handler_stream");
            if (!check(tc, "ms_open_handler_stream(NULL out)",
                       ms_open_handler_stream(nullptr, nullptr),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_open_handler_stream");
            if (!check(tc, "ms_open_handler_stream(already set)",
                       ms_open_handler_stream(nullptr, &already),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_open_stream")) {
            ms_stream_t *stream = nullptr;
            ms_stream_t *already = reinterpret_cast<ms_stream_t *>(1);
            hit("ms_open_stream");
            if (!check(tc, "ms_open_stream(NULL db)",
                       ms_open_stream(nullptr, &stream), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_open_stream");
            if (!check(tc, "ms_open_stream(NULL out)",
                       ms_open_stream(nullptr, nullptr), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_open_stream");
            if (!check(tc, "ms_open_stream(already set)",
                       ms_open_stream(nullptr, &already), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
        }

        if (isTarget("ms_scan_stream")) {
            hit("ms_scan_stream");
            if (!check(tc, "ms_scan_stream(NULL data)",
                       ms_scan_stream(nullptr, nullptr, 1u, nullptr, ids,
                                      &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_scan_stream");
            if (!check(tc, "ms_scan_stream(NULL count)",
                       ms_scan_stream(nullptr, data, 1u, nullptr, ids,
                                      nullptr, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_scan_stream");
            if (!check(tc, "ms_scan_stream(NULL ids with capacity)",
                       ms_scan_stream(nullptr, data, 1u, nullptr, nullptr,
                                      &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_flush_stream")) {
            hit("ms_flush_stream");
            if (!check(tc, "ms_flush_stream(NULL count)",
                       ms_flush_stream(nullptr, nullptr, ids, nullptr, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_flush_stream");
            if (!check(tc, "ms_flush_stream(NULL ids with capacity)",
                       ms_flush_stream(nullptr, nullptr, nullptr, &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_close_stream")) {
            hit("ms_close_stream");
            if (!check(tc, "ms_close_stream(NULL count)",
                       ms_close_stream(nullptr, nullptr, ids, nullptr, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_close_stream");
            if (!check(tc, "ms_close_stream(NULL ids with capacity)",
                       ms_close_stream(nullptr, nullptr, nullptr, &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_scan_expr") || isTarget("ms_scan_expr_stream") ||
            isTarget("ms_scan_expr_stream_flush") ||
            isTarget("ms_scan_rule_stream") || isTarget("ms_reset_stream") ||
            isTarget("ms_close_handler_stream")) {
            if (isTarget("ms_scan_expr")) {
                hit("ms_scan_expr");
                if (!check(tc, "ms_scan_expr(NULL handler)",
                           ms_scan_expr(nullptr, data, 1u, ids, &count, 4u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_scan_expr");
                if (!check(tc, "ms_scan_expr(NULL data)",
                           ms_scan_expr(nullptr, nullptr, 1u, ids, &count, 4u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
            if (isTarget("ms_scan_expr_stream")) {
                hit("ms_scan_expr_stream");
                if (!check(tc, "ms_scan_expr_stream(NULL stream)",
                           ms_scan_expr_stream(nullptr, data, 1u, ids, &count,
                                               4u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_scan_expr_stream");
                if (!check(tc, "ms_scan_expr_stream(NULL ids with capacity)",
                           ms_scan_expr_stream(nullptr, data, 1u, nullptr,
                                               &count, 4u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
            if (isTarget("ms_scan_expr_stream_flush")) {
                hit("ms_scan_expr_stream_flush");
                if (!check(tc, "ms_scan_expr_stream_flush(NULL stream)",
                           ms_scan_expr_stream_flush(nullptr, ids, &count, 4u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_scan_expr_stream_flush");
                if (!check(tc, "ms_scan_expr_stream_flush(NULL ids cap)",
                           ms_scan_expr_stream_flush(nullptr, nullptr, &count,
                                                     4u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
            if (isTarget("ms_scan_rule_stream")) {
                hit("ms_scan_rule_stream");
                if (!check(tc, "ms_scan_rule_stream(NULL stream)",
                           ms_scan_rule_stream(nullptr, data, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
                hit("ms_scan_rule_stream");
                if (!check(tc, "ms_scan_rule_stream(NULL data)",
                           ms_scan_rule_stream(nullptr, nullptr, 1u),
                           MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
            if (isTarget("ms_reset_stream")) {
                hit("ms_reset_stream");
                if (!check(tc, "ms_reset_stream(NULL)",
                           ms_reset_stream(nullptr), MS_INVALID_PARA, error)) {
                    ok = false;
                }
            }
            if (isTarget("ms_close_handler_stream")) {
                hit("ms_close_handler_stream");
                if (!check(tc, "ms_close_handler_stream(NULL)",
                           ms_close_handler_stream(nullptr), MS_INVALID_PARA,
                           error)) {
                    ok = false;
                }
            }
        }

        if (isTarget("ms_compile_scan_ac_combinations")) {
            hit("ms_compile_scan_ac_combinations");
            if (!check(tc, "ms_compile_scan_ac_combinations(NULL handler)",
                       ms_compile_scan_ac_combinations(nullptr, data, 1u, ids,
                                                       &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_compile_eval_combinations")) {
            hit("ms_compile_eval_combinations");
            if (!check(tc, "ms_compile_eval_combinations(NULL handler)",
                       ms_compile_eval_combinations(nullptr, ids, 1u, ids,
                                                    &count, 4u),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_logical_parse")) {
            hit("ms_logical_parse");
            if (!check(tc, "ms_logical_parse(NULL logical)",
                       ms_logical_parse(nullptr, &tree), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_logical_parse");
            if (!check(tc, "ms_logical_parse(NULL tree)",
                       ms_logical_parse("0&1", nullptr), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_logical_parse");
            if (!check(tc, "ms_logical_parse(invalid)",
                       ms_logical_parse("0&", &tree), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
        }

        if (isTarget("ms_logical_tree_destroy")) {
            hit("ms_logical_tree_destroy");
            ms_logical_tree_destroy(nullptr);
        }

        if (isTarget("ms_logical_set")) {
            hit("ms_logical_set");
            if (!check(tc, "ms_logical_set(NULL vector)",
                       ms_logical_set(nullptr, 1u, 0u, 1u), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
            hit("ms_logical_set");
            if (!check(tc, "ms_logical_set(key overflow)",
                       ms_logical_set(logical, 1u, 1u, 1u), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
        }

        if (isTarget("ms_logical_get")) {
            hit("ms_logical_get");
            if (!check(tc, "ms_logical_get(NULL vector)",
                       ms_logical_get(nullptr, 1u, 0u, &value),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_logical_get");
            if (!check(tc, "ms_logical_get(NULL value)",
                       ms_logical_get(logical, 1u, 0u, nullptr),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_logical_clear")) {
            hit("ms_logical_clear");
            if (!check(tc, "ms_logical_clear(NULL vector)",
                       ms_logical_clear(nullptr, 1u), MS_INVALID_PARA,
                       error)) {
                ok = false;
            }
        }

        if (isTarget("ms_logical_eval")) {
            hit("ms_logical_eval");
            if (!check(tc, "ms_logical_eval(NULL tree)",
                       ms_logical_eval(nullptr, logical, 1u, &matched),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_logical_eval");
            if (!check(tc, "ms_logical_eval(empty tree)",
                       ms_logical_eval(&tree, logical, 1u, &matched),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        if (isTarget("ms_logical_eval_inplace")) {
            hit("ms_logical_eval_inplace");
            if (!check(tc, "ms_logical_eval_inplace(NULL tree)",
                       ms_logical_eval_inplace(nullptr, logical, 1u, &matched),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
            hit("ms_logical_eval_inplace");
            if (!check(tc, "ms_logical_eval_inplace(empty tree)",
                       ms_logical_eval_inplace(&tree, logical, 1u, &matched),
                       MS_INVALID_PARA, error)) {
                ok = false;
            }
        }

        return ok;
    }

    bool allocatorApis(const FuzzTestCase &tc, std::string &error) {
        struct SetterResult {
            const char *name;
            int result;
        };
        const SetterResult results[] = {
            {"ms_set_allocator", ms_set_allocator(nullptr, nullptr, nullptr)},
            {"ms_set_database_allocator",
             ms_set_database_allocator(nullptr, nullptr, nullptr)},
            {"ms_set_scratch_allocator",
             ms_set_scratch_allocator(nullptr, nullptr, nullptr)},
            {"ms_set_stream_allocator",
             ms_set_stream_allocator(nullptr, nullptr, nullptr)},
            {"ms_set_misc_allocator",
             ms_set_misc_allocator(nullptr, nullptr, nullptr)}
        };
        bool ok = true;
        for (const auto &entry : results) {
            hit(entry.name);
            if (!check(tc, entry.name, entry.result, MS_SUCCESS, error)) {
                ok = false;
            }
        }
        return ok;
    }

    bool databaseAc(const FuzzTestCase &tc, std::string &error) {
        auto expr = pointers(tc);
        std::vector<ms_expr_ext_t> ext(expr.size());
        ms_database_t *empty = nullptr;
        ms_database_t *extDb = nullptr;
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

        ext.back().flags = MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET;
        ext.back().max_offset = tc.data.size();
        hit("ms_compile_database_multi_ext");
        ret = ms_compile_database_multi_ext(&extDb, MS_MATCHER_TYPE_AC, 0, 0,
                                            expr.data(), tc.flags.data(),
                                            tc.ids.data(), ext.data(),
                                            expr.size());
        if (!check(tc, "ms_compile_database_multi_ext", ret,
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_free_database");
        ret = ms_free_database(extDb);
        extDb = nullptr;
        if (!check(tc, "ms_free_database(ext)", ret, MS_SUCCESS, error)) {
            goto cleanup;
        }

        hit("ms_compile_database_multi");
        ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                        expr.data(), tc.flags.data(),
                                        tc.ids.data(), expr.size());
        if (!check(tc, "ms_compile_database_multi", ret, MS_SUCCESS, error)) {
            goto cleanup;
        }
        hit("ms_alloc_scratch");
        if (!check(tc, "ms_alloc_scratch", ms_alloc_scratch(db, &scratch),
                   MS_SUCCESS, error)) {
            goto cleanup;
        }
        ok = true;
        hit("ms_scan_database");
        ret = ms_scan_database(db, tc.data.data(), tc.data.size(), scratch,
                               ids, &count, 64u);
        if (!checkScan(tc, "ms_scan_database", ret, error)) {
            ok = false;
        }
        hit("ms_get_match_events");
        if (!check(tc, "ms_get_match_events",
                   ms_get_match_events(scratch, nullptr, &eventCount, 0),
                   MS_SUCCESS, error)) {
            ok = false;
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
            ok = false;
        }
        count = 0;
        hit("ms_flush_stream");
        ret = ms_flush_stream(stream, scratch, ids, &count, 64u);
        if (!checkScan(tc, "ms_flush_stream", ret, error)) {
            ok = false;
        }
        count = 0;
        hit("ms_close_stream");
        ret = ms_close_stream(stream, scratch, ids, &count, 64u);
        stream = nullptr;
        if (!checkScan(tc, "ms_close_stream", ret, error)) {
            ok = false;
        }

        {
            const bool removeCombination = (tc.round & 1u) != 0u;
            const std::size_t ruleIndex = removeCombination
                                              ? tc.expressions.size() - 1u
                                              : 0u;
            ms_rule_t rule = {
                tc.ids[ruleIndex],
                removeCombination
                    ? 0u
                    : static_cast<uint32_t>(tc.expressions[ruleIndex].size()),
                removeCombination
                    ? nullptr
                    : reinterpret_cast<uint8_t *>(const_cast<char *>(
                          tc.expressions[ruleIndex].c_str()))};
            hit("ms_find_database_rule");
            ret = ms_find_database_rule(db, &rule);
            if (!checkScan(tc, "ms_find_database_rule", ret, error)) {
                ok = false;
            }

            hit("ms_open_stream");
            if (!check(tc, "ms_open_stream(stale)", ms_open_stream(db, &stream),
                       MS_SUCCESS, error)) {
                ok = false;
            }
            rule.alias = tc.ids[ruleIndex];
            hit("ms_remove_database_rule");
            ret = ms_remove_database_rule(db, &rule, 1u);
            if (!check(tc, "ms_remove_database_rule", ret, MS_SUCCESS,
                       error)) {
                ok = false;
            }
            if (stream != nullptr && ret == MS_SUCCESS) {
                count = 0;
                hit("ms_scan_stream");
                ret = ms_scan_stream(stream, tc.data.data(), tc.data.size(),
                                     scratch, ids, &count, 64u);
                if (!check(tc, "ms_scan_stream(stale)", ret,
                           MS_STREAM_STALE, error)) {
                    ok = false;
                }
                hit("ms_reset_stream");
                ret = ms_reset_stream(stream);
                if (!check(tc, "ms_reset_stream(stale refresh)", ret,
                           MS_SUCCESS, error)) {
                    ok = false;
                }
                count = 0;
                hit("ms_scan_stream");
                ret = ms_scan_stream(stream, tc.data.data(), tc.data.size(),
                                     scratch, ids, &count, 64u);
                if (!checkScan(tc, "ms_scan_stream(after reset)", ret,
                               error)) {
                    ok = false;
                }
                count = 0;
                hit("ms_close_stream");
                ret = ms_close_stream(stream, scratch, ids, &count, 64u);
                stream = nullptr;
                if (!checkScan(tc, "ms_close_stream(stale)", ret, error)) {
                    ok = false;
                }
            }
        }

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
        if (extDb != nullptr) {
            hit("ms_free_database");
            (void)ms_free_database(extDb);
        }
        return ok;
    }

    bool legacyAc(const FuzzTestCase &tc, std::string &error) {
        auto expr = pointers(tc);
        std::vector<ms_expr_ext_t> ext(expr.size());
        ms_handler_t handler = {};
        ms_handler_t extHandler = {};
        ms_handler_t addHandler = {};
        ms_stream_t *stream = nullptr;
        uint32_t ids[64] = {};
        uint32_t combinations[64] = {};
        uint32_t count = 0;
        uint32_t combinationCount = 0;
        int ret;
        bool ok = false;
        bool variantsOk = true;

        ext.back().flags = MS_EXT_FLAG_MAX_OFFSET;
        ext.back().max_offset = tc.data.size();

        hit("ms_handler_init");
        ret = ms_handler_init(&extHandler, MS_MATCHER_TYPE_AC, 0, 0);
        if (!check(tc, "ms_handler_init(AC ext)", ret, MS_SUCCESS, error)) {
            variantsOk = false;
        } else {
            hit("ms_compile_multi_ext");
            ret = ms_compile_multi_ext(&extHandler, expr.data(), tc.flags.data(),
                                       tc.ids.data(), ext.data(), expr.size());
            if (!check(tc, "ms_compile_multi_ext", ret, MS_SUCCESS, error)) {
                variantsOk = false;
            }
            hit("ms_handler_destroy");
            ret = ms_handler_destroy(&extHandler);
            if (!check(tc, "ms_handler_destroy(AC ext)", ret,
                       MS_SUCCESS, error)) {
                variantsOk = false;
            }
        }

        hit("ms_handler_init");
        ret = ms_handler_init(&addHandler, MS_MATCHER_TYPE_AC, 0, 0);
        if (!check(tc, "ms_handler_init(AC add)", ret, MS_SUCCESS, error)) {
            variantsOk = false;
        } else {
            hit("ms_add_exprs");
            ret = ms_add_exprs(&addHandler, expr.data(), tc.ids.data(),
                               tc.flags.data(), expr.size());
            if (!check(tc, "ms_add_exprs", ret, MS_SUCCESS, error)) {
                variantsOk = false;
            }
            hit("ms_handler_destroy");
            ret = ms_handler_destroy(&addHandler);
            if (!check(tc, "ms_handler_destroy(AC add)", ret,
                       MS_SUCCESS, error)) {
                variantsOk = false;
            }
        }

        hit("ms_handler_init");
        if (!check(tc, "ms_handler_init(AC)",
                   ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0),
                   MS_SUCCESS, error)) {
            return false;
        }
        hit("ms_compile_multi");
        ret = ms_compile_multi(&handler, expr.data(), tc.flags.data(),
                               tc.ids.data(), expr.size());
        if (!check(tc, "ms_compile_multi", ret, MS_SUCCESS, error)) {
            goto cleanup;
        }
        ok = true;
        hit("ms_scan_expr");
        ret = ms_scan_expr(&handler, tc.data.data(), tc.data.size(), ids,
                           &count, 64u);
        if (!checkScan(tc, "ms_scan_expr", ret, error)) {
            ok = false;
        }
        hit("ms_compile_scan_ac_combinations");
        ret = ms_compile_scan_ac_combinations(&handler, tc.data.data(),
                                              tc.data.size(), combinations,
                                              &combinationCount, 64u);
        if (!checkScan(tc, "ms_compile_scan_ac_combinations", ret, error)) {
            ok = false;
        }
        hit("ms_compile_eval_combinations");
        ret = ms_compile_eval_combinations(&handler, ids, count, combinations,
                                           &combinationCount, 64u);
        if (!checkScan(tc, "ms_compile_eval_combinations", ret, error)) {
            ok = false;
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
            ok = false;
        }
        hit("ms_reset_stream");
        if (!check(tc, "ms_reset_stream", ms_reset_stream(stream),
                   MS_SUCCESS, error)) {
            ok = false;
        }
        count = 0;
        hit("ms_scan_expr_stream");
        ret = ms_scan_expr_stream(stream, tc.data.data(), tc.data.size(), ids,
                                  &count, 64u);
        if (!checkScan(tc, "ms_scan_expr_stream", ret, error)) {
            ok = false;
        }
        hit("ms_scan_expr_stream_flush");
        ret = ms_scan_expr_stream_flush(stream, ids, &count, 64u);
        if (!checkScan(tc, "ms_scan_expr_stream_flush", ret, error)) {
            ok = false;
        }
        hit("ms_close_handler_stream");
        if (!check(tc, "ms_close_handler_stream",
                   ms_close_handler_stream(stream), MS_SUCCESS, error)) {
            ok = false;
        }
        stream = nullptr;

        {
            ms_rule_t comboRule = {tc.ids.back(), 0u, nullptr};
            hit("ms_remove_rule");
            ret = ms_remove_rule(&handler, &comboRule, 1u);
            if (!checkScan(tc, "ms_remove_rule(combination)", ret, error)) {
                ok = false;
            }
        }

        {
            const char *comboExpr[] = {"a", "b", "100&101", "!100"};
            const uint32_t comboIds[] = {100u, 101u, 200u, 201u};
            const uint32_t comboFlags[] = {
                MS_FLAG_NONE,
                MS_FLAG_NONE,
                MS_FLAG_COMBINATION,
                MS_FLAG_COMBINATION
            };
            const uint8_t comboData[] = {'a', 'b'};
            ms_handler_t comboHandler = {};
            ms_stream_t *comboStream = nullptr;
            uint32_t comboOut[8] = {};
            uint32_t comboOutCount = 0;

            hit("ms_handler_init");
            ret = ms_handler_init(&comboHandler, MS_MATCHER_TYPE_AC, 0, 0);
            if (!check(tc, "ms_handler_init(combo)", ret, MS_SUCCESS, error)) {
                ok = false;
            } else {
                hit("ms_compile_multi");
                ret = ms_compile_multi(&comboHandler, comboExpr, comboFlags,
                                       comboIds, 4u);
                if (!check(tc, "ms_compile_multi(combo)", ret, MS_SUCCESS,
                           error)) {
                    ok = false;
                } else {
                    hit("ms_scan_expr");
                    comboOutCount = 0;
                    ret = ms_scan_expr(&comboHandler, comboData,
                                       sizeof(comboData), comboOut,
                                       &comboOutCount, 8u);
                    if (!checkScan(tc, "ms_scan_expr(combo)", ret, error)) {
                        ok = false;
                    }

                    hit("ms_compile_scan_ac_combinations");
                    comboOutCount = 0;
                    ret = ms_compile_scan_ac_combinations(
                        &comboHandler, comboData, sizeof(comboData), comboOut,
                        &comboOutCount, 8u);
                    if (!checkScan(tc, "ms_compile_scan_ac_combinations(combo)",
                                   ret, error)) {
                        ok = false;
                    }

                    hit("ms_open_handler_stream");
                    ret = ms_open_handler_stream(&comboHandler, &comboStream);
                    if (!check(tc, "ms_open_handler_stream(combo)", ret,
                               MS_SUCCESS, error)) {
                        ok = false;
                    } else {
                        hit("ms_scan_expr_stream");
                        comboOutCount = 0;
                        ret = ms_scan_expr_stream(comboStream, comboData,
                                                  sizeof(comboData), comboOut,
                                                  &comboOutCount, 8u);
                        if (!checkScan(tc, "ms_scan_expr_stream(combo)", ret,
                                       error)) {
                            ok = false;
                        }
                        hit("ms_scan_expr_stream_flush");
                        comboOutCount = 0;
                        ret = ms_scan_expr_stream_flush(comboStream, comboOut,
                                                        &comboOutCount, 8u);
                        if (!checkScan(tc, "ms_scan_expr_stream_flush(combo)",
                                       ret, error)) {
                            ok = false;
                        }
                        hit("ms_close_handler_stream");
                        ret = ms_close_handler_stream(comboStream);
                        comboStream = nullptr;
                        if (!check(tc, "ms_close_handler_stream(combo)", ret,
                                   MS_SUCCESS, error)) {
                            ok = false;
                        }
                    }
                }
                if (comboStream != nullptr) {
                    hit("ms_close_handler_stream");
                    (void)ms_close_handler_stream(comboStream);
                }
                hit("ms_handler_destroy");
                ret = ms_handler_destroy(&comboHandler);
                if (!check(tc, "ms_handler_destroy(combo)", ret, MS_SUCCESS,
                           error)) {
                    ok = false;
                }
            }
        }

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
        hit("ms_handler_destroy");
        ret = ms_handler_destroy(&handler);
        if (!check(tc, "ms_handler_destroy(AC idempotent)", ret,
                   MS_SUCCESS, error)) {
            ok = false;
        }
        return ok && variantsOk;
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
        {
            ms_rule_t rule = rules[tc.round % rules.size()];
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
        {
            ms_rule_t rule = rules[tc.round % rules.size()];
            hit("ms_find_database_rule");
            if (!check(tc, "ms_find_database_rule(HASH)",
                       ms_find_database_rule(db, &rule),
                       MS_SUCCESS, error)) {
                ok = false;
            }
        }
        hit("ms_free_database");
        ret = ms_free_database(db);
        return check(tc, "ms_free_database(HASH)", ret, MS_SUCCESS, error) && ok;
    }

    bool acRuleApis(const FuzzTestCase &tc, std::string &error) {
        ms_handler_t handler = {};
        std::vector<std::string> values;
        std::vector<ms_rule_t> rules;
        bool ok = false;
        int ret;

        for (std::size_t i = 0; i + 1u < tc.expressions.size(); ++i) {
            values.push_back(tc.expressions[i]);
        }
        for (std::size_t i = 0; i < values.size(); ++i) {
            rules.push_back(ms_rule_t{
                tc.ids[i], static_cast<uint32_t>(values[i].size()),
                reinterpret_cast<uint8_t *>(
                    const_cast<char *>(values[i].c_str()))
            });
        }

        hit("ms_handler_init");
        if (!check(tc, "ms_handler_init(AC rules)",
                   ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0),
                   MS_SUCCESS, error)) {
            return false;
        }
        hit("ms_add_rule");
        ret = ms_add_rule(&handler, rules.data(),
                          static_cast<uint32_t>(rules.size()));
        if (!check(tc, "ms_add_rule(AC)", ret, MS_SUCCESS, error)) {
            goto cleanup;
        }

        {
            ms_rule_t query = {};
            query.key = const_cast<uint8_t *>(tc.data.data());
            query.key_len = static_cast<uint32_t>(tc.data.size());
            hit("ms_find_rule");
            ret = ms_find_rule(&handler, &query);
            if (!checkScan(tc, "ms_find_rule(AC)", ret, error)) {
                goto cleanup;
            }
        }

        hit("ms_remove_rule");
        ret = ms_remove_rule(&handler, &rules[tc.round % rules.size()], 1u);
        if (!check(tc, "ms_remove_rule(AC)", ret, MS_SUCCESS, error)) {
            goto cleanup;
        }
        ok = true;

cleanup:
        hit("ms_handler_destroy");
        ret = ms_handler_destroy(&handler);
        if (!check(tc, "ms_handler_destroy(AC rules)", ret,
                   MS_SUCCESS, error)) {
            ok = false;
        }
        return ok;
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
    std::string targetApi_;
};

} // namespace

std::unique_ptr<Runner> createRunner() {
    return std::unique_ptr<Runner>(new MsRunner());
}
