#pragma once

#include <string>

namespace freight {

// Every quote ID generator this example ships satisfies the same
// unwritten contract: next_id() never returns an empty string, and never
// repeats within the lifetime of the process. QuoteIdGeneratorContractTest
// in tests/quote_id_generator_test.cpp proves that contract once, as a
// typed test, against every concrete generator below, instead of writing
// the same two checks out by hand for each one.
class QuoteIdGenerator {
public:
    virtual ~QuoteIdGenerator() = default;
    virtual std::string next_id() = 0;
};

// Formats ids as "Q-0001", "Q-0002", and so on, from a counter that is
// process-wide rather than per-instance. That mirrors how a real backing
// store would hand out quote numbers: a database sequence lives in the
// database, not in whichever QuoteIntake object happens to be running, so
// constructing a second SequentialQuoteIdGenerator does not restart
// numbering from zero. reset_sequence_for_test() exists only so tests can
// put that shared counter back to a known value; production code never
// calls it.
class SequentialQuoteIdGenerator : public QuoteIdGenerator {
public:
    std::string next_id() override;

    static void reset_sequence_for_test(int next_value = 1);

private:
    static int next_value_;
};

// A second, independent generator: ids like "EXP-7", "EXP-8", counted from
// an instance member rather than a shared sequence. QuoteIntake only
// depends on the QuoteIdGenerator interface, so either generator plugs in
// without the intake code changing, and the typed test suite proves both
// honor the same contract despite the different numbering strategy
// underneath.
class PrefixedQuoteIdGenerator : public QuoteIdGenerator {
public:
    explicit PrefixedQuoteIdGenerator(std::string prefix = "EXP");

    std::string next_id() override;

private:
    std::string prefix_;
    int next_value_ = 1;
};

}  // namespace freight
