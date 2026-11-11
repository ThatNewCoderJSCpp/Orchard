#ifndef ORCHARD_EDITOR_SOUND_PLAYER_HPP
#define ORCHARD_EDITOR_SOUND_PLAYER_HPP

#include <orchard/scene.hpp>
#include <orchard/scripting.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace orchard {
namespace editor {

class SoundPlayer {
public:
    SoundPlayer();
    ~SoundPlayer();
    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;

    void set_folder(const std::string& folder);
    void apply(const std::vector<SoundCommand>& commands, const Scene& scene);
    void update(const Scene& scene, Vec3 listener, Vec3 right, bool flat, std::vector<int>& finished);
    void stop_all();
    void preview(const std::string& clip, float volume = 1.0f);
    std::size_t playing() const;
    const std::string& error() const { return m_error; }
    std::string resolve(const std::string& clip) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
    std::string m_error;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_SOUND_PLAYER_HPP
