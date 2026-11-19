#include "miniq/exchange_service.hpp"

namespace miniq {

std::optional<RejectReason> ExchangeService::validate(const OrderRequest& request) const {
    if (request.quantity <= 0) {
        return RejectReason::InvalidQuantity;
    }
    if (request.type == OrderType::Limit) {
        if (!request.limitPrice.has_value() || *request.limitPrice <= 0) {
            return RejectReason::InvalidPrice;
        }
    }
    // Market orders: a supplied limitPrice is simply ignored (documented
    // policy — see docs/DESIGN_DECISIONS.md), not rejected, so a client
    // that always populates the field does not get spurious rejections.
    return std::nullopt;
}

SubmitResult ExchangeService::submit(const OrderRequest& request) {
    SubmitResult result;

    if (auto reason = validate(request)) {
        result.rejectReason = *reason;
        eventBus_.publish(OrderRejected{request.id, request.symbol, *reason});
        return result;
    }

    if (knownOrderIds_.contains(request.id.value())) {
        result.rejectReason = RejectReason::DuplicateOrderId;
        eventBus_.publish(OrderRejected{request.id, request.symbol, result.rejectReason});
        return result;
    }

    const Quantity currentPosition = positionService_.positionFor(request.symbol);
    if (auto reason = riskEngine_.evaluate(request, currentPosition)) {
        result.rejectReason = *reason;
        eventBus_.publish(OrderRejected{request.id, request.symbol, *reason});
        return result;
    }

    knownOrderIds_.insert(request.id.value());

    Order order{request.id,       request.symbol, request.side,  request.type,
                request.quantity, request.limitPrice, clock_.nextSequence()};

    eventBus_.publish(OrderAccepted{request.id, request.symbol});

    matchingEngine_.submit(std::move(order), result.trades, clock_);
    result.accepted = true;

    for (const auto& trade : result.trades) {
        // Single-account simplification: PositionService tracks net
        // exposure for the submitting flow only. The resting order's
        // owner is not modeled as a separate account in this project, so
        // only the aggressor (incoming) side updates position — this
        // matches what MaxPositionRule checks pre-trade for the
        // submitter. See docs/DESIGN_DECISIONS.md.
        positionService_.adjust(trade.symbol, trade.aggressorSide, trade.quantity);
        eventBus_.publish(TradeExecuted{trade});
    }

    eventBus_.publish(BookUpdated{request.symbol, matchingEngine_.bestBid(request.symbol),
                                   matchingEngine_.bestAsk(request.symbol)});

    return result;
}

CancelResult ExchangeService::cancel(const Symbol& symbol, OrderId id) {
    const CancelResult result = matchingEngine_.cancel(symbol, id);
    if (result == CancelResult::Cancelled) {
        eventBus_.publish(OrderCancelled{id, symbol});
        eventBus_.publish(
            BookUpdated{symbol, matchingEngine_.bestBid(symbol), matchingEngine_.bestAsk(symbol)});
    }
    return result;
}

} // namespace miniq
