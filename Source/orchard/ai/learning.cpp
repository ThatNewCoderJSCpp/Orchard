#include <orchard/ai/learning.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace orchard {
namespace ai {

namespace {

double activate(Activation a, double x) {
    switch (a) {
    case Activation::tanh: return std::tanh(x);
    case Activation::sigmoid: return 1.0 / (1.0 + std::exp(-x));
    case Activation::relu: return x > 0.0 ? x : 0.0;
    case Activation::leaky_relu: return x > 0.0 ? x : 0.01 * x;
    case Activation::linear:
    case Activation::softmax: return x;
    }
    return x;
}

double derivative(Activation a, double y) {
    switch (a) {
    case Activation::tanh: return 1.0 - y * y;
    case Activation::sigmoid: return y * (1.0 - y);
    case Activation::relu: return y > 0.0 ? 1.0 : 0.0;
    case Activation::leaky_relu: return y > 0.0 ? 1.0 : 0.01;
    case Activation::linear:
    case Activation::softmax: return 1.0;
    }
    return 1.0;
}

void softmax(std::vector<double>& v) {
    if (v.empty()) return;
    const double m = *std::max_element(v.begin(), v.end());
    double sum = 0.0;
    for (double& x : v) {
        x = std::exp(x - m);
        sum += x;
    }
    for (double& x : v) x /= sum;
}

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace

const char* to_string(Activation a) {
    switch (a) {
    case Activation::tanh: return "tanh";
    case Activation::sigmoid: return "sigmoid";
    case Activation::relu: return "relu";
    case Activation::leaky_relu: return "leaky-relu";
    case Activation::linear: return "linear";
    case Activation::softmax: return "softmax";
    }
    return "tanh";
}

std::optional<Activation> parse_activation(const std::string& t) {
    for (Activation a : {Activation::tanh, Activation::sigmoid, Activation::relu, Activation::leaky_relu, Activation::linear, Activation::softmax})
        if (t == to_string(a)) return a;
    if (t == "leaky_relu") return Activation::leaky_relu;
    return std::nullopt;
}

NeuralNet::NeuralNet(const std::vector<int>& layers, Activation hidden, Activation output, std::uint64_t seed) {
    build(layers, hidden, output, seed);
}

void NeuralNet::build(const std::vector<int>& layers, Activation hidden, Activation output, std::uint64_t seed) {
    m_layers.clear();
    for (int l : layers)
        if (l > 0) m_layers.push_back(l);
    m_hidden = hidden;
    m_output = output;
    m_weights.clear();
    m_biases.clear();
    Random r(seed);
    for (std::size_t i = 1; i < m_layers.size(); ++i) {
        const int in = m_layers[i - 1], out = m_layers[i];
        const double scale = std::sqrt(2.0 / static_cast<double>(in + out));
        std::vector<double> w(static_cast<std::size_t>(in * out));
        for (double& x : w) x = r.gaussian(0.0, scale);
        m_weights.push_back(std::move(w));
        m_biases.push_back(std::vector<double>(static_cast<std::size_t>(out), 0.0));
    }
}

std::vector<double> NeuralNet::run(const std::vector<double>& inputs) const {
    std::vector<double> a(static_cast<std::size_t>(this->inputs()), 0.0);
    for (std::size_t i = 0; i < a.size() && i < inputs.size(); ++i) a[i] = inputs[i];
    for (std::size_t l = 0; l < m_weights.size(); ++l) {
        const int in = m_layers[l], out = m_layers[l + 1];
        std::vector<double> next(static_cast<std::size_t>(out));
        const bool last = l + 1 == m_weights.size();
        for (int o = 0; o < out; ++o) {
            double s = m_biases[l][static_cast<std::size_t>(o)];
            for (int i = 0; i < in; ++i) s += a[static_cast<std::size_t>(i)] * m_weights[l][static_cast<std::size_t>(o * in + i)];
            next[static_cast<std::size_t>(o)] = activate(last ? m_output : m_hidden, s);
        }
        if (last && m_output == Activation::softmax) softmax(next);
        a = std::move(next);
    }
    return a;
}

double NeuralNet::train(const std::vector<double>& inputs, const std::vector<double>& targets, double rate) {
    if (m_weights.empty()) return 0.0;
    std::vector<std::vector<double>> acts;
    std::vector<double> a(static_cast<std::size_t>(this->inputs()), 0.0);
    for (std::size_t i = 0; i < a.size() && i < inputs.size(); ++i) a[i] = inputs[i];
    acts.push_back(a);
    for (std::size_t l = 0; l < m_weights.size(); ++l) {
        const int in = m_layers[l], out = m_layers[l + 1];
        std::vector<double> next(static_cast<std::size_t>(out));
        const bool last = l + 1 == m_weights.size();
        for (int o = 0; o < out; ++o) {
            double s = m_biases[l][static_cast<std::size_t>(o)];
            for (int i = 0; i < in; ++i) s += acts.back()[static_cast<std::size_t>(i)] * m_weights[l][static_cast<std::size_t>(o * in + i)];
            next[static_cast<std::size_t>(o)] = activate(last ? m_output : m_hidden, s);
        }
        if (last && m_output == Activation::softmax) softmax(next);
        acts.push_back(next);
    }
    const std::vector<double>& out = acts.back();
    std::vector<double> delta(out.size());
    double loss = 0.0;
    for (std::size_t o = 0; o < out.size(); ++o) {
        const double t = o < targets.size() ? targets[o] : 0.0;
        const double e = out[o] - t;
        loss += e * e;
        delta[o] = m_output == Activation::softmax ? e : e * derivative(m_output, out[o]);
    }
    for (std::size_t l = m_weights.size(); l-- > 0;) {
        const int in = m_layers[l], outn = m_layers[l + 1];
        std::vector<double> prev(static_cast<std::size_t>(in), 0.0);
        for (int o = 0; o < outn; ++o) {
            const double d = delta[static_cast<std::size_t>(o)];
            for (int i = 0; i < in; ++i) {
                double& w = m_weights[l][static_cast<std::size_t>(o * in + i)];
                prev[static_cast<std::size_t>(i)] += w * d;
                w -= rate * d * acts[l][static_cast<std::size_t>(i)];
            }
            m_biases[l][static_cast<std::size_t>(o)] -= rate * d;
        }
        if (l > 0)
            for (int i = 0; i < in; ++i) prev[static_cast<std::size_t>(i)] *= derivative(m_hidden, acts[l][static_cast<std::size_t>(i)]);
        delta = std::move(prev);
    }
    return loss / std::max<std::size_t>(1, out.size());
}

double NeuralNet::train_batch(const std::vector<std::vector<double>>& inputs, const std::vector<std::vector<double>>& targets, int epochs, double rate) {
    double loss = 0.0;
    for (int e = 0; e < epochs; ++e) {
        loss = 0.0;
        for (std::size_t i = 0; i < inputs.size() && i < targets.size(); ++i) loss += train(inputs[i], targets[i], rate);
        loss /= std::max<std::size_t>(1, inputs.size());
    }
    return loss;
}

std::size_t NeuralNet::weight_count() const {
    std::size_t n = 0;
    for (std::size_t l = 0; l < m_weights.size(); ++l) n += m_weights[l].size() + m_biases[l].size();
    return n;
}

std::vector<double> NeuralNet::genome() const {
    std::vector<double> g;
    for (std::size_t l = 0; l < m_weights.size(); ++l) {
        g.insert(g.end(), m_weights[l].begin(), m_weights[l].end());
        g.insert(g.end(), m_biases[l].begin(), m_biases[l].end());
    }
    return g;
}

void NeuralNet::set_genome(const std::vector<double>& g) {
    std::size_t k = 0;
    for (std::size_t l = 0; l < m_weights.size(); ++l) {
        for (double& w : m_weights[l]) w = k < g.size() ? g[k++] : w;
        for (double& b : m_biases[l]) b = k < g.size() ? g[k++] : b;
    }
}

void NeuralNet::mutate(Random& random, double rate, double amount) {
    for (std::size_t l = 0; l < m_weights.size(); ++l) {
        for (double& w : m_weights[l])
            if (random.chance(rate)) w += random.gaussian(0.0, amount);
        for (double& b : m_biases[l])
            if (random.chance(rate)) b += random.gaussian(0.0, amount);
    }
}

std::string NeuralNet::save() const {
    std::ostringstream o;
    o.precision(17);
    o << "orchard-net 1 " << to_string(m_hidden) << " " << to_string(m_output) << " " << m_layers.size();
    for (int l : m_layers) o << " " << l;
    o << "\n";
    for (double v : genome()) o << v << " ";
    o << "\n";
    return o.str();
}

std::optional<NeuralNet> NeuralNet::load(const std::string& text) {
    std::istringstream in(text);
    std::string magic, hidden, output;
    int version = 0;
    std::size_t n = 0;
    in >> magic >> version >> hidden >> output >> n;
    if (magic != "orchard-net" || version != 1 || n > 64) return std::nullopt;
    std::vector<int> layers(n);
    for (int& l : layers) in >> l;
    const auto h = parse_activation(hidden), o = parse_activation(output);
    if (!in || !h || !o) return std::nullopt;
    NeuralNet net(layers, *h, *o);
    std::vector<double> g;
    double v = 0.0;
    while (in >> v) g.push_back(v);
    if (g.size() != net.weight_count()) return std::nullopt;
    net.set_genome(g);
    return net;
}

GeneticAlgorithm::GeneticAlgorithm(int genes, GeneticSettings s, std::uint64_t seed) : m_genes(std::max(1, genes)), m_s(s), m_random(seed) {
    m_population.resize(static_cast<std::size_t>(std::max(2, s.population)));
    for (auto& g : m_population) {
        g.resize(static_cast<std::size_t>(m_genes));
        for (double& x : g) x = m_random.range(s.low, s.high);
    }
    m_fitness.assign(m_population.size(), 0.0);
}

void GeneticAlgorithm::seed_population(const std::vector<double>& genome, double jitter) {
    for (auto& g : m_population)
        for (std::size_t i = 0; i < g.size(); ++i) g[i] = (i < genome.size() ? genome[i] : 0.0) + m_random.gaussian(0.0, jitter);
    if (!m_population.empty()) m_population[0] = genome;
    m_population[0].resize(static_cast<std::size_t>(m_genes), 0.0);
}

void GeneticAlgorithm::set_fitness(int i, double f) {
    if (i < 0 || i >= size()) return;
    m_fitness[static_cast<std::size_t>(i)] = f;
    if (f > m_best_fitness) {
        m_best_fitness = f;
        m_best = m_population[static_cast<std::size_t>(i)];
    }
}

int GeneticAlgorithm::pick() {
    int best = static_cast<int>(m_random.index(m_population.size()));
    for (int t = 1; t < std::max(1, m_s.tournament); ++t) {
        const int c = static_cast<int>(m_random.index(m_population.size()));
        if (m_fitness[static_cast<std::size_t>(c)] > m_fitness[static_cast<std::size_t>(best)]) best = c;
    }
    return best;
}

void GeneticAlgorithm::evolve() {
    std::vector<std::size_t> order(m_population.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return m_fitness[a] > m_fitness[b]; });
    std::vector<std::vector<double>> next;
    for (int e = 0; e < std::min(m_s.elite, size()); ++e) next.push_back(m_population[order[static_cast<std::size_t>(e)]]);
    while (next.size() < m_population.size()) {
        const auto& a = m_population[static_cast<std::size_t>(pick())];
        const auto& b = m_population[static_cast<std::size_t>(pick())];
        std::vector<double> child = a;
        if (m_random.chance(m_s.crossover))
            for (std::size_t i = 0; i < child.size(); ++i)
                if (m_random.chance(0.5)) child[i] = b[i];
        for (double& x : child)
            if (m_random.chance(m_s.mutation_rate)) x += m_random.gaussian(0.0, m_s.mutation_amount);
        next.push_back(std::move(child));
    }
    m_population = std::move(next);
    std::fill(m_fitness.begin(), m_fitness.end(), 0.0);
    ++m_generation;
}

void GeneticAlgorithm::run(const std::function<double(const std::vector<double>&)>& fitness, int generations) {
    for (int g = 0; g < generations; ++g) {
        for (int i = 0; i < size(); ++i) set_fitness(i, fitness(m_population[static_cast<std::size_t>(i)]));
        if (g + 1 < generations) evolve();
    }
}

QLearner::QLearner(int actions, double a, double g, double e, std::uint64_t seed) : alpha(a), gamma(g), epsilon(e), m_actions(std::max(1, actions)), m_random(seed) {}

std::vector<double>& QLearner::row(const std::string& state) {
    auto& r = m_table[state];
    if (static_cast<int>(r.size()) != m_actions) r.resize(static_cast<std::size_t>(m_actions), 0.0);
    return r;
}

int QLearner::best(const std::string& state) const {
    const auto it = m_table.find(state);
    if (it == m_table.end() || it->second.empty()) return 0;
    return static_cast<int>(std::max_element(it->second.begin(), it->second.end()) - it->second.begin());
}

int QLearner::choose(const std::string& state) {
    row(state);
    const int a = m_random.chance(epsilon) ? static_cast<int>(m_random.index(static_cast<std::size_t>(m_actions))) : best(state);
    epsilon = std::max(epsilon_min, epsilon * epsilon_decay);
    return a;
}

void QLearner::learn(const std::string& state, int action, double reward, const std::string& next, bool terminal) {
    if (action < 0 || action > 4096) return;
    if (action >= m_actions) m_actions = action + 1;
    const std::vector<double>& n = row(next);
    const double future = terminal ? 0.0 : *std::max_element(n.begin(), n.end());
    double& q = row(state)[static_cast<std::size_t>(action)];
    q += alpha * (reward + gamma * future - q);
}

double QLearner::value(const std::string& state, int action) const {
    const auto it = m_table.find(state);
    if (it == m_table.end() || action < 0 || action >= static_cast<int>(it->second.size())) return 0.0;
    return it->second[static_cast<std::size_t>(action)];
}

std::string QLearner::save() const {
    std::ostringstream o;
    o.precision(17);
    o << "orchard-q 1 " << m_actions << "\n";
    for (const auto& [s, r] : m_table) {
        o << s.size() << " " << s;
        for (double v : r) o << " " << v;
        o << "\n";
    }
    return o.str();
}

bool QLearner::load(const std::string& text) {
    std::istringstream in(text);
    std::string magic;
    int version = 0, actions = 0;
    in >> magic >> version >> actions;
    if (magic != "orchard-q" || version != 1 || actions <= 0) return false;
    m_actions = actions;
    m_table.clear();
    std::size_t len = 0;
    while (in >> len) {
        in.get();
        std::string state(len, ' ');
        in.read(&state[0], static_cast<std::streamsize>(len));
        std::vector<double> r(static_cast<std::size_t>(actions));
        for (double& v : r) in >> v;
        m_table[state] = r;
    }
    return true;
}

void MarkovChain::train(const std::vector<std::string>& seq) {
    std::vector<std::string> padded(static_cast<std::size_t>(m_order), "");
    padded.insert(padded.end(), seq.begin(), seq.end());
    padded.push_back("");
    for (std::size_t i = static_cast<std::size_t>(m_order); i < padded.size(); ++i) {
        std::vector<std::string> ctx(padded.begin() + static_cast<std::ptrdiff_t>(i) - m_order, padded.begin() + static_cast<std::ptrdiff_t>(i));
        ++m_table[ctx][padded[i]];
    }
}

void MarkovChain::train_text(const std::string& text, bool chars) {
    if (!chars) {
        std::string sentence;
        for (char c : text) {
            sentence += c;
            if (c == '\n') {
                train(words(sentence));
                sentence.clear();
            }
        }
        if (!sentence.empty()) train(words(sentence));
        return;
    }
    for (const std::string& w : words(text)) {
        std::vector<std::string> letters;
        for (char c : w) letters.push_back(std::string(1, c));
        train(letters);
    }
}

std::string MarkovChain::next(Random& random, const std::vector<std::string>& context) const {
    std::vector<std::string> ctx(static_cast<std::size_t>(m_order), "");
    for (std::size_t i = 0; i < context.size() && i < ctx.size(); ++i) ctx[ctx.size() - 1 - i] = context[context.size() - 1 - i];
    const auto it = m_table.find(ctx);
    if (it == m_table.end()) return std::string();
    int total = 0;
    for (const auto& [w, n] : it->second) total += n;
    int r = static_cast<int>(random.integer(1, std::max(1, total)));
    for (const auto& [w, n] : it->second) {
        r -= n;
        if (r <= 0) return w;
    }
    return std::string();
}

std::vector<std::string> MarkovChain::generate(Random& random, int max_length, const std::vector<std::string>& start) const {
    std::vector<std::string> out = start;
    while (static_cast<int>(out.size()) < max_length) {
        const std::string n = next(random, out);
        if (n.empty()) break;
        out.push_back(n);
    }
    return out;
}

std::string MarkovChain::generate_text(Random& random, int max_length, bool chars) const {
    std::string text;
    for (const std::string& w : generate(random, max_length)) text += (chars || text.empty() ? "" : " ") + w;
    return text;
}

} // namespace ai
} // namespace orchard
