#pragma once
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "miniq/order.hpp"
#include "miniq/types.hpp"

namespace miniq {

// Context passed to a risk rule. Bundled into a struct rather than a long
// parameter list — cheap to extend without breaking every IRiskRule.
struct RiskContext {
    const OrderRequest& request;
    // Current net position for request.symbol, before this order.
    Quantity currentPosition{0};
};

// IRiskRule — Strategy pattern. Each rule answers one question:
// "would accepting this request violate my policy?"
// LSP contract: every implementation must be a pure, side-effect-free
// check that returns std::nullopt (pass) or a RejectReason (fail) purely
// as a function of the given context — callers may substitute any rule
// without changing RiskEngine's correctness.
class IRiskRule {
public:
    virtual ~IRiskRule() = default;
    [[nodiscard]] virtual std::optional<RejectReason> evaluate(const RiskContext& ctx) const = 0;
};

// Rejects orders whose quantity exceeds a configured maximum.
class MaxQuantityRule final : public IRiskRule {
public:
    explicit MaxQuantityRule(Quantity maxQuantity) : maxQuantity_(maxQuantity) {}
    [[nodiscard]] std::optional<RejectReason> evaluate(const RiskContext& ctx) const override;

private:
    Quantity maxQuantity_;
};

// Rejects LIMIT orders whose notional (price * quantity) exceeds a
// configured maximum. Policy for MARKET orders: notional is unknown until
// match time, so this rule intentionally does not evaluate market orders
// — that ambiguity is documented rather than hidden (see
// docs/DESIGN_DECISIONS.md and the LLD guide's risk section).
class MaxNotionalRule final : public IRiskRule {
public:
    explicit MaxNotionalRule(std::int64_t maxNotional) : maxNotional_(maxNotional) {}
    [[nodiscard]] std::optional<RejectReason> evaluate(const RiskContext& ctx) const override;

private:
    std::int64_t maxNotional_;
};

// Rejects an order if the resulting position (assuming a full fill) would
// exceed a configured absolute bound. This is a conservative pre-trade
// check: it assumes worst case (full fill) rather than tracking partial
// fills speculatively.
class MaxPositionRule final : public IRiskRule {
public:
    explicit MaxPositionRule(Quantity maxAbsPosition) : maxAbsPosition_(maxAbsPosition) {}
    [[nodiscard]] std::optional<RejectReason> evaluate(const RiskContext& ctx) const override;

private:
    Quantity maxAbsPosition_;
};

// RiskEngine — owns a heterogeneous set of rules via unique_ptr<IRiskRule>
// (exclusive ownership: no other component shares or outlives these rules)
// and evaluates them in deterministic (insertion) order, returning the
// first rejection. This demonstrates OCP: new rules plug in without any
// change to RiskEngine itself.
class RiskEngine {
public:
    void addRule(std::unique_ptr<IRiskRule> rule);

    [[nodiscard]] std::optional<RejectReason> evaluate(const OrderRequest& request,
                                                         Quantity currentPosition) const;

private:
    std::vector<std::unique_ptr<IRiskRule>> rules_;
};

} // namespace miniq
