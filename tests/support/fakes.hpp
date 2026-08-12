#pragma once

#include "development_card_deck.hpp"
#include "dice.hpp"
#include "game_controller.hpp"

#include <deque>
#include <memory>
#include <stdexcept>
#include <vector>

namespace test {

struct DiceState {
    std::deque<size_t> rolls;
    size_t calls{};
};

class ScriptedDice final : public ivv::game::IDice {
public:
    explicit ScriptedDice(std::shared_ptr<DiceState> state) : state_(std::move(state)) {}
    size_t Roll() override {
        ++state_->calls;
        if (state_->rolls.empty()) throw std::logic_error("ScriptedDice queue is empty");
        const size_t result = state_->rolls.front();
        state_->rolls.pop_front();
        return result;
    }
private:
    std::shared_ptr<DiceState> state_;
};

struct DeckState {
    std::deque<ivv::catan::DevelopmentCard> cards;
    size_t draws{};
};

class FixedDevelopmentCardDeck final : public ivv::catan::IDevelopmentCardDeck {
public:
    explicit FixedDevelopmentCardDeck(std::shared_ptr<DeckState> state) : state_(std::move(state)) {}
    bool Empty() const override { return state_->cards.empty(); }
    ivv::catan::DevelopmentCard Draw() override {
        if (Empty()) throw std::logic_error("FixedDevelopmentCardDeck is empty");
        ++state_->draws;
        auto card = state_->cards.front();
        state_->cards.pop_front();
        return card;
    }
private:
    std::shared_ptr<DeckState> state_;
};

struct ControlledGame {
    std::shared_ptr<DiceState> first_die = std::make_shared<DiceState>();
    std::shared_ptr<DiceState> second_die = std::make_shared<DiceState>();
    std::shared_ptr<DeckState> deck = std::make_shared<DeckState>();
    std::unique_ptr<ivv::catan::GameController> game;

    ControlledGame(std::vector<std::string> names,
                   std::deque<size_t> first = {}, std::deque<size_t> second = {},
                   std::deque<ivv::catan::DevelopmentCard> cards = {}) {
        first_die->rolls = std::move(first);
        second_die->rolls = std::move(second);
        deck->cards = std::move(cards);
        ivv::catan::GameController::Dependencies deps;
        deps.dice[0] = std::make_unique<ScriptedDice>(first_die);
        deps.dice[1] = std::make_unique<ScriptedDice>(second_die);
        deps.development_cards = std::make_unique<FixedDevelopmentCardDeck>(deck);
        game = std::make_unique<ivv::catan::GameController>(std::move(names), std::move(deps));
    }
};

} // namespace test
