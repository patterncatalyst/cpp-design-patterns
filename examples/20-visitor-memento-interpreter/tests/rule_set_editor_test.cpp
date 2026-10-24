#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include "rule_expr.hpp"
#include "rule_set_editor.hpp"
#include "shipping_domain.hpp"

namespace {

using shipping::DestinationZone;
using shipping::Shipment;

TEST(RuleSetEditorTest, AddAndRemoveRuleTracksPresence) {
    rules::RuleSetEditor editor;
    editor.add_rule("over-weight", rules::weight_above(10.0));

    EXPECT_TRUE(editor.has_rule("over-weight"));
    EXPECT_EQ(editor.rule_count(), 1u);

    EXPECT_TRUE(editor.remove_rule("over-weight"));
    EXPECT_FALSE(editor.has_rule("over-weight"));
    EXPECT_FALSE(editor.remove_rule("over-weight"));
}

TEST(RuleSetEditorTest, EvaluateRuleThrowsForMissingLabel) {
    rules::RuleSetEditor editor;
    const Shipment shipment{1.0, DestinationZone::Local, 0};

    EXPECT_THROW(editor.evaluate_rule("ghost-rule", shipment), std::out_of_range);
}

TEST(RuleSetEditorTest, EvaluateRuleWalksTheStoredExpression) {
    rules::RuleSetEditor editor;
    editor.add_rule("national-heavy",
                     rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National)));

    const Shipment matching{12.0, DestinationZone::National, 0};
    const Shipment not_matching{5.0, DestinationZone::National, 0};

    EXPECT_TRUE(editor.evaluate_rule("national-heavy", matching));
    EXPECT_FALSE(editor.evaluate_rule("national-heavy", not_matching));
}

TEST(RuleSetEditorTest, SaveAndRestoreRevertsWholeState) {
    rules::RuleSetEditor editor;
    editor.set_draft_name("Q4-international-filters");
    editor.add_rule("high-value-national",
                     rules::rule_or(rules::rule_and(rules::weight_above(10.0),
                                                     rules::zone_is(DestinationZone::National)),
                                    rules::declared_value_above(50000)));
    editor.set_published(false);

    const Shipment shipment{5.0, DestinationZone::National, 80000};
    const bool result_before_mutation = editor.evaluate_rule("high-value-national", shipment);

    rules::RuleSetEditor::Snapshot snapshot = editor.save();

    editor.set_draft_name("DRAFT-EDIT");
    editor.add_rule("temp-rule", rules::weight_above(999.0));
    editor.remove_rule("high-value-national");
    editor.set_published(true);

    ASSERT_EQ(editor.draft_name(), "DRAFT-EDIT");
    ASSERT_EQ(editor.rule_count(), 1u);
    ASSERT_TRUE(editor.is_published());

    editor.restore(std::move(snapshot));

    EXPECT_EQ(editor.draft_name(), "Q4-international-filters");
    EXPECT_EQ(editor.rule_count(), 1u);
    EXPECT_FALSE(editor.is_published());
    EXPECT_TRUE(editor.has_rule("high-value-national"));
    EXPECT_FALSE(editor.has_rule("temp-rule"));
    EXPECT_EQ(editor.evaluate_rule("high-value-national", shipment), result_before_mutation);
}

#if __cplusplus >= 202302L
TEST(RuleSetEditorTest, TryEvaluateRuleReturnsTypedErrorForMissingLabel) {
    rules::RuleSetEditor editor;
    const Shipment shipment{1.0, DestinationZone::Local, 0};

    const auto result = editor.try_evaluate_rule("ghost-rule", shipment);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "no such rule: ghost-rule");
}

TEST(RuleSetEditorTest, TryEvaluateRuleReturnsValueForPresentLabel) {
    rules::RuleSetEditor editor;
    editor.add_rule("over-weight", rules::weight_above(10.0));
    const Shipment shipment{12.0, DestinationZone::Local, 0};

    const auto result = editor.try_evaluate_rule("over-weight", shipment);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}
#endif

}  // namespace
