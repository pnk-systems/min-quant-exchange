#include "miniq/risk.hpp"

#include <limits>

namespace miniq {

std::optional<RejectReason> MaxQuantityRule::evaluate(const RiskContext& ctx) const {
    if (ctx.request.quantity > maxQuantity_) {
        return RejectReason::MaxQuantityExceeded;
    }
    return std::nullopt;
}

std::optional<RejectReason> MaxNotionalRule::evaluate(const RiskContext& ctx) const {
    if (ctx.request.type != OrderType::Limit) {
        // Explicit policy: notional is undefined for a market order before
        // matching, so this rule does not apply to market orders.
        return std::nullopt;
    }
    // Widen to int64 multiplication explicitly and check for overflow
    // before it happens, rather than relying on wraparound behavior.
    const auto price = ctx.request.limitPrice.value_or(0);
    const auto quantity = ctx.request.quantity;
    if (price != 0 && quantity > (std::numeric_limits<std::int64_t>::max() / price)) {
        return RejectReason::MaxNotionalExceeded; // would overflow -> treat as exceeding
    }
    const std::int64_t notional = price * quantity;
    if (notional > maxNotional_) {
        return RejectReason::MaxNotionalExceeded;
    }
    return std::nullopt;
}

std::optional<RejectReason> MaxPositionRule::evaluate(const RiskContext& ctx) const {
    const Quantity signedQty =
        (ctx.request.side == Side::Buy) ? ctx.request.quantity : -ctx.request.quantity;
    const Quantity projected = ctx.currentPosition + signedQty;
    if (projected > maxAbsPosition_ || projected < -maxAbsPosition_) {
        return RejectReason::MaxPositionExceeded;
    }
    return std::nullopt;
}

void RiskEngine::addRule(std::unique_ptr<IRiskRule> rule) {
    rules_.push_back(std::move(rule));
}

std::optional<RejectReason> RiskEngine::evaluate(const OrderRequest& request,
                                                   Quantity currentPosition) const {
    const RiskContext ctx{request, currentPosition};
    for (const auto& rule : rules_) {
        if (auto reason = rule->evaluate(ctx)) {
            return reason;
        }
    }
    return std::nullopt;
}

} // namespace miniq
