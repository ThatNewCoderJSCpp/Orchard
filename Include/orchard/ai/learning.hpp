#ifndef ORCHARD_AI_LEARNING_HPP
#define ORCHARD_AI_LEARNING_HPP

#include "../random.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchard {
namespace ai {

enum class Activation { tanh, sigmoid, relu, leaky_relu, linear, softmax };

const char* to_string(Activation a);
std::optional<Activation> parse_activation(const std::string& text);

class NeuralNet {
public:
    NeuralNet() = default;
    NeuralNet(const std::vector<int>& layers, Activation hidden = Activation::tanh, Activation output = Activation::sigmoid, std::uint64_t seed = 1);

    void build(const std::vector<int>& layers, Activation hidden = Activation::tanh, Activation output = Activation::sigmoid, std::uint64_t seed = 1);
    std::vector<double> run(const std::vector<double>& inputs) const;
    double train(const std::vector<double>& inputs, const std::vector<double>& targets, double rate = 0.05);
    double train_batch(const std::vector<std::vector<double>>& inputs, const std::vector<std::vector<double>>& targets, int epochs, double rate = 0.05);
    int inputs() const { return m_layers.empty() ? 0 : m_layers.front(); }
    int outputs() const { return m_layers.empty() ? 0 : m_layers.back(); }
    const std::vector<int>& layers() const { return m_layers; }

    std::vector<double> genome() const;
    void set_genome(const std::vector<double>& weights);
    std::size_t weight_count() const;
    void mutate(Random& random, double rate, double amount);

    std::string save() const;
    static std::optional<NeuralNet> load(const std::string& text);

private:
    std::vector<int> m_layers;
    std::vector<std::vector<double>> m_weights;
    std::vector<std::vector<double>> m_biases;
    Activation m_hidden = Activation::tanh;
    Activation m_output = Activation::sigmoid;
};

struct GeneticSettings {
    int population = 50;
    double mutation_rate = 0.1;
    double mutation_amount = 0.3;
    double crossover = 0.7;
    int elite = 2;
    int tournament = 3;
    double low = -1.0;
    double high = 1.0;
};

class GeneticAlgorithm {
public:
    GeneticAlgorithm(int genes, GeneticSettings settings = GeneticSettings(), std::uint64_t seed = 1);
    void seed_population(const std::vector<double>& genome, double jitter = 0.1);
    const std::vector<std::vector<double>>& population() const { return m_population; }
    std::vector<double>& genome(int i) { return m_population[static_cast<std::size_t>(i)]; }
    void set_fitness(int i, double fitness);
    void evolve();
    void run(const std::function<double(const std::vector<double>&)>& fitness, int generations);
    const std::vector<double>& best() const { return m_best; }
    double best_fitness() const { return m_best_fitness; }
    int generation() const { return m_generation; }
    int size() const { return static_cast<int>(m_population.size()); }

private:
    int pick();

    int m_genes = 0;
    GeneticSettings m_s;
    Random m_random;
    std::vector<std::vector<double>> m_population;
    std::vector<double> m_fitness;
    std::vector<double> m_best;
    double m_best_fitness = -1e300;
    int m_generation = 0;
};

class QLearner {
public:
    QLearner(int actions = 4, double alpha = 0.2, double gamma = 0.9, double epsilon = 0.1, std::uint64_t seed = 1);
    int choose(const std::string& state);
    int best(const std::string& state) const;
    void learn(const std::string& state, int action, double reward, const std::string& next, bool terminal = false);
    double value(const std::string& state, int action) const;
    void set_actions(int n) { m_actions = std::max(1, n); }
    int actions() const { return m_actions; }
    double alpha;
    double gamma;
    double epsilon;
    double epsilon_decay = 1.0;
    double epsilon_min = 0.01;
    std::size_t states() const { return m_table.size(); }
    std::string save() const;
    bool load(const std::string& text);

private:
    std::vector<double>& row(const std::string& state);
    int m_actions = 4;
    Random m_random;
    std::unordered_map<std::string, std::vector<double>> m_table;
};

class MarkovChain {
public:
    explicit MarkovChain(int order = 1) : m_order(std::max(1, order)) {}
    void train(const std::vector<std::string>& sequence);
    void train_text(const std::string& text, bool by_characters = false);
    std::vector<std::string> generate(Random& random, int max_length, const std::vector<std::string>& start = {}) const;
    std::string generate_text(Random& random, int max_length, bool by_characters = false) const;
    std::string next(Random& random, const std::vector<std::string>& context) const;
    int order() const { return m_order; }
    std::size_t states() const { return m_table.size(); }

private:
    int m_order;
    std::map<std::vector<std::string>, std::map<std::string, int>> m_table;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_LEARNING_HPP
