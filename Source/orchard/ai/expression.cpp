#include <orchard/ai/expression.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace orchard {
namespace ai {

struct Expression::Node {
    enum class Kind { number, call, unary, binary } kind = Kind::number;
    double value = 0.0;
    std::string name;
    char op = 0;
    std::string op2;
    std::vector<std::shared_ptr<const Node>> args;
};

namespace {

using NodePtr = std::shared_ptr<const Expression::Node>;

class Parser {
public:
    explicit Parser(const std::string& t) : s(t) {}

    NodePtr parse(std::string* error) {
        NodePtr n = parse_or();
        skip();
        if (!err.empty() || i != s.size()) {
            if (error) *error = err.empty() ? "unexpected '" + s.substr(i) + "'" : err;
            return nullptr;
        }
        return n;
    }

private:
    void skip() {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    bool eat(const char* tok) {
        skip();
        const std::size_t n = std::char_traits<char>::length(tok);
        if (s.compare(i, n, tok) == 0) {
            if (std::isalpha(static_cast<unsigned char>(tok[0])) && i + n < s.size() && (std::isalnum(static_cast<unsigned char>(s[i + n])) || s[i + n] == '_')) return false;
            i += n;
            return true;
        }
        return false;
    }

    NodePtr bin(const std::string& op, NodePtr a, NodePtr b) {
        auto n = std::make_shared<Expression::Node>();
        n->kind = Expression::Node::Kind::binary;
        n->op2 = op;
        n->args = {std::move(a), std::move(b)};
        return n;
    }

    NodePtr parse_or() {
        NodePtr a = parse_and();
        while (a && (eat("||") || eat("or"))) a = bin("||", a, parse_and());
        return a;
    }

    NodePtr parse_and() {
        NodePtr a = parse_cmp();
        while (a && (eat("&&") || eat("and"))) a = bin("&&", a, parse_cmp());
        return a;
    }

    NodePtr parse_cmp() {
        NodePtr a = parse_add();
        for (;;) {
            if (!a) return a;
            if (eat("<=")) a = bin("<=", a, parse_add());
            else if (eat(">=")) a = bin(">=", a, parse_add());
            else if (eat("==")) a = bin("==", a, parse_add());
            else if (eat("!=")) a = bin("!=", a, parse_add());
            else if (eat("<")) a = bin("<", a, parse_add());
            else if (eat(">")) a = bin(">", a, parse_add());
            else return a;
        }
    }

    NodePtr parse_add() {
        NodePtr a = parse_mul();
        for (;;) {
            if (!a) return a;
            if (eat("+")) a = bin("+", a, parse_mul());
            else if (eat("-")) a = bin("-", a, parse_mul());
            else return a;
        }
    }

    NodePtr parse_mul() {
        NodePtr a = parse_unary();
        for (;;) {
            if (!a) return a;
            if (eat("*")) a = bin("*", a, parse_unary());
            else if (eat("/")) a = bin("/", a, parse_unary());
            else if (eat("%")) a = bin("%", a, parse_unary());
            else return a;
        }
    }

    NodePtr parse_unary() {
        if (eat("-")) {
            auto n = std::make_shared<Expression::Node>();
            n->kind = Expression::Node::Kind::unary;
            n->op = '-';
            n->args = {parse_unary()};
            return n;
        }
        if (eat("!") || eat("not")) {
            auto n = std::make_shared<Expression::Node>();
            n->kind = Expression::Node::Kind::unary;
            n->op = '!';
            n->args = {parse_unary()};
            return n;
        }
        return parse_pow();
    }

    NodePtr parse_pow() {
        NodePtr a = parse_atom();
        if (a && eat("^")) a = bin("^", a, parse_unary());
        return a;
    }

    NodePtr parse_atom() {
        skip();
        if (i >= s.size()) {
            err = "the expression ends too early";
            return nullptr;
        }
        if (eat("(")) {
            NodePtr n = parse_or();
            if (!eat(")")) err = "missing )";
            return n;
        }
        const char c = s[i];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            const char* begin = s.c_str() + i;
            char* end = nullptr;
            const double v = std::strtod(begin, &end);
            i += static_cast<std::size_t>(end - begin);
            auto n = std::make_shared<Expression::Node>();
            n->value = v;
            return n;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t j = i;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' || s[j] == '.')) ++j;
            auto n = std::make_shared<Expression::Node>();
            n->kind = Expression::Node::Kind::call;
            n->name = s.substr(i, j - i);
            i = j;
            if (n->name == "true" || n->name == "false") {
                auto b = std::make_shared<Expression::Node>();
                b->value = n->name == "true" ? 1.0 : 0.0;
                return b;
            }
            if (eat("(")) {
                if (!eat(")")) {
                    do {
                        n->args.push_back(parse_or());
                        if (!err.empty()) return nullptr;
                    } while (eat(","));
                    if (!eat(")")) err = "missing ) after the arguments of " + n->name;
                }
            }
            return n;
        }
        err = std::string("unexpected '") + c + "'";
        return nullptr;
    }

    const std::string& s;
    std::size_t i = 0;
    std::string err;
};

bool builtin(const std::string& name, const std::vector<double>& a, double& out) {
    auto arg = [&](std::size_t k, double d) { return k < a.size() ? a[k] : d; };
    if (name == "min") {
        out = a.empty() ? 0.0 : *std::min_element(a.begin(), a.end());
    } else if (name == "max") {
        out = a.empty() ? 0.0 : *std::max_element(a.begin(), a.end());
    } else if (name == "abs") {
        out = std::fabs(arg(0, 0.0));
    } else if (name == "clamp") {
        out = std::clamp(arg(0, 0.0), arg(1, 0.0), std::max(arg(1, 0.0), arg(2, 1.0)));
    } else if (name == "sqrt") {
        out = std::sqrt(std::max(0.0, arg(0, 0.0)));
    } else if (name == "pow") {
        out = std::pow(arg(0, 0.0), arg(1, 1.0));
    } else if (name == "lerp") {
        out = arg(0, 0.0) + (arg(1, 1.0) - arg(0, 0.0)) * arg(2, 0.0);
    } else if (name == "step") {
        out = arg(0, 0.0) >= arg(1, 0.5) ? 1.0 : 0.0;
    } else if (name == "inverse") {
        out = 1.0 - std::clamp(arg(0, 0.0), 0.0, 1.0);
    } else if (name == "near") {
        out = std::clamp(1.0 - arg(0, 0.0) / std::max(1e-6, arg(1, 10.0)), 0.0, 1.0);
    } else if (name == "far") {
        out = std::clamp(arg(0, 0.0) / std::max(1e-6, arg(1, 10.0)), 0.0, 1.0);
    } else if (name == "logistic") {
        out = 1.0 / (1.0 + std::exp(-arg(1, 10.0) * (arg(0, 0.0) - arg(2, 0.5))));
    } else if (name == "quadratic") {
        out = std::clamp(arg(0, 0.0), 0.0, 1.0);
        out *= out;
    } else if (name == "floor") {
        out = std::floor(arg(0, 0.0));
    } else if (name == "ceil") {
        out = std::ceil(arg(0, 0.0));
    } else if (name == "round") {
        out = std::round(arg(0, 0.0));
    } else if (name == "sin") {
        out = std::sin(arg(0, 0.0));
    } else if (name == "cos") {
        out = std::cos(arg(0, 0.0));
    } else if (name == "pi") {
        out = 3.14159265358979323846;
    } else {
        return false;
    }
    return true;
}

double eval(const Expression::Node& n, const ExpressionLookup& lookup) {
    using K = Expression::Node::Kind;
    switch (n.kind) {
    case K::number: return n.value;
    case K::unary: {
        const double v = eval(*n.args[0], lookup);
        return n.op == '-' ? -v : (v != 0.0 ? 0.0 : 1.0);
    }
    case K::binary: {
        const std::string& op = n.op2;
        const double a = eval(*n.args[0], lookup);
        if (op == "&&") return a != 0.0 && eval(*n.args[1], lookup) != 0.0 ? 1.0 : 0.0;
        if (op == "||") return a != 0.0 || eval(*n.args[1], lookup) != 0.0 ? 1.0 : 0.0;
        const double b = eval(*n.args[1], lookup);
        if (op == "+") return a + b;
        if (op == "-") return a - b;
        if (op == "*") return a * b;
        if (op == "/") return b != 0.0 ? a / b : 0.0;
        if (op == "%") return b != 0.0 ? std::fmod(a, b) : 0.0;
        if (op == "^") return std::pow(a, b);
        if (op == "<") return a < b ? 1.0 : 0.0;
        if (op == ">") return a > b ? 1.0 : 0.0;
        if (op == "<=") return a <= b ? 1.0 : 0.0;
        if (op == ">=") return a >= b ? 1.0 : 0.0;
        if (op == "==") return std::fabs(a - b) < 1e-9 ? 1.0 : 0.0;
        if (op == "!=") return std::fabs(a - b) >= 1e-9 ? 1.0 : 0.0;
        return 0.0;
    }
    case K::call: {
        std::vector<double> args;
        args.reserve(n.args.size());
        for (const auto& a : n.args) args.push_back(eval(*a, lookup));
        double out = 0.0;
        if (lookup && lookup(n.name, args, out)) return out;
        if (builtin(n.name, args, out)) return out;
        return 0.0;
    }
    }
    return 0.0;
}

} // namespace

Expression::Expression() = default;
Expression::~Expression() = default;
Expression::Expression(const Expression&) = default;
Expression& Expression::operator=(const Expression&) = default;
Expression::Expression(Expression&&) noexcept = default;
Expression& Expression::operator=(Expression&&) noexcept = default;

std::optional<Expression> Expression::parse(const std::string& text, std::string* error) {
    Parser p(text);
    NodePtr root = p.parse(error);
    if (!root) return std::nullopt;
    Expression e;
    e.m_root = std::move(root);
    e.m_text = text;
    return e;
}

double Expression::evaluate(const ExpressionLookup& lookup) const {
    return m_root ? eval(*m_root, lookup) : 0.0;
}

bool Expression::empty() const {
    return !m_root;
}

double evaluate_expression(const std::string& text, const ExpressionLookup& lookup, std::string* error) {
    const std::optional<Expression> e = Expression::parse(text, error);
    return e ? e->evaluate(lookup) : 0.0;
}

} // namespace ai
} // namespace orchard
