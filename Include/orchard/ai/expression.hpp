#ifndef ORCHARD_AI_EXPRESSION_HPP
#define ORCHARD_AI_EXPRESSION_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace orchard {
namespace ai {

using ExpressionLookup = std::function<bool(const std::string& name, const std::vector<double>& args, double& out)>;

class Expression {
public:
    Expression();
    ~Expression();
    Expression(const Expression&);
    Expression& operator=(const Expression&);
    Expression(Expression&&) noexcept;
    Expression& operator=(Expression&&) noexcept;

    static std::optional<Expression> parse(const std::string& text, std::string* error = nullptr);
    double evaluate(const ExpressionLookup& lookup) const;
    const std::string& text() const { return m_text; }
    bool empty() const;

    struct Node;

private:
    std::shared_ptr<const Node> m_root;
    std::string m_text;
};

double evaluate_expression(const std::string& text, const ExpressionLookup& lookup, std::string* error = nullptr);

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_EXPRESSION_HPP
