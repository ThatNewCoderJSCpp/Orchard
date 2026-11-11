#include "sound_player.hpp"

#include <fizmo/../audio.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace orchard {
namespace editor {

namespace fs = std::filesystem;
namespace fa = fizmo::audio;

struct SoundPlayer::Impl {
    struct Voice {
        fa::Mixer::VoiceId id = 0;
        SoundCommand cmd;
        bool paused = false;
    };

    std::unique_ptr<fa::Mixer> mixer;
    bool failed = false;
    std::string folder;
    std::map<std::string, fa::Sound> cache;
    std::map<std::string, bool> missing;
    std::map<int, Voice> voices;
    fa::Mixer::VoiceId preview = 0;

    bool ready(std::string& error) {
        if (mixer) return true;
        if (failed) return false;
        mixer = std::make_unique<fa::Mixer>(48000, 2);
        std::string why;
        if (!mixer->start(fa::StreamConfig(), &why)) {
            error = why.empty() ? "no sound output" : why;
            failed = true;
            mixer.reset();
            return false;
        }
        return true;
    }

    fa::Sound load(const std::string& path, std::string& error) {
        const auto it = cache.find(path);
        if (it != cache.end()) return it->second;
        if (missing.count(path)) return nullptr;
        fa::AudioBuffer buffer;
        std::string why;
        if (path.empty() || !fa::load_audio(path, buffer, &why)) {
            error = "can't play " + path + (why.empty() ? std::string() : ": " + why);
            missing[path] = true;
            return nullptr;
        }
        if (buffer.channels() != 2 || buffer.sample_rate() != 48000) buffer = buffer.converted(2, 48000);
        fa::Sound s = fa::make_sound(std::move(buffer));
        cache[path] = s;
        return s;
    }
};

SoundPlayer::SoundPlayer() : m(std::make_unique<Impl>()) {}

SoundPlayer::~SoundPlayer() {
    stop_all();
}

void SoundPlayer::set_folder(const std::string& folder) {
    if (folder != m->folder) {
        m->cache.clear();
        m->missing.clear();
    }
    m->folder = folder;
}

std::string SoundPlayer::resolve(const std::string& clip) const {
    if (clip.empty()) return std::string();
    std::error_code ec;
    const fs::path p(clip);
    if (p.is_absolute() && fs::is_regular_file(p, ec)) return p.string();
    for (const char* sub : {"", "Assets", "Assets/Sounds", "Sounds"}) {
        const fs::path q = fs::path(m->folder) / sub / clip;
        if (fs::is_regular_file(q, ec)) return q.string();
    }
    return (fs::path(m->folder) / clip).string();
}

void SoundPlayer::apply(const std::vector<SoundCommand>& commands, const Scene& scene) {
    static_cast<void>(scene);
    for (const SoundCommand& c : commands) {
        switch (c.op) {
        case SoundOp::play: {
            if (!m->ready(m_error)) break;
            const fa::Sound s = m->load(resolve(c.clip), m_error);
            if (!s) break;
            fa::PlayOptions o;
            o.gain = c.positional ? 0.0f : c.volume;
            o.speed = c.pitch;
            o.loop = c.loop;
            Impl::Voice v;
            v.cmd = c;
            v.id = m->mixer->play(s, o);
            m->voices[c.id] = v;
            break;
        }
        case SoundOp::stop: {
            const auto it = m->voices.find(c.id);
            if (it != m->voices.end()) {
                if (m->mixer) m->mixer->stop(it->second.id, 0.05);
                m->voices.erase(it);
            }
            break;
        }
        case SoundOp::volume: {
            const auto it = m->voices.find(c.id);
            if (it != m->voices.end()) it->second.cmd.volume = c.volume;
            break;
        }
        case SoundOp::pitch: {
            const auto it = m->voices.find(c.id);
            if (it != m->voices.end() && m->mixer) {
                it->second.cmd.pitch = c.pitch;
                m->mixer->set_speed(it->second.id, c.pitch);
            }
            break;
        }
        case SoundOp::pause:
        case SoundOp::resume: {
            const auto it = m->voices.find(c.id);
            if (it != m->voices.end() && m->mixer) {
                it->second.paused = c.op == SoundOp::pause;
                m->mixer->set_paused(it->second.id, it->second.paused);
            }
            break;
        }
        case SoundOp::stop_all: stop_all(); break;
        }
    }
}

void SoundPlayer::update(const Scene& scene, Vec3 listener, Vec3 right, bool flat, std::vector<int>& finished) {
    if (!m->mixer) {
        for (const auto& [id, v] : m->voices) finished.push_back(id);
        m->voices.clear();
        return;
    }
    const Vec3 r = length(right) > 1e-5f ? normalize(right) : Vec3{1.0f, 0.0f, 0.0f};
    for (auto it = m->voices.begin(); it != m->voices.end();) {
        Impl::Voice& v = it->second;
        if (!v.paused && !m->mixer->is_playing(v.id)) {
            finished.push_back(it->first);
            it = m->voices.erase(it);
            continue;
        }
        float gain = v.cmd.volume;
        float pan = 0.0f;
        if (v.cmd.positional) {
            Vec3 at = v.cmd.at;
            if (scene.alive(v.cmd.follow)) at = mul_point(scene.world_matrix(v.cmd.follow), Vec3{});
            Vec3 d = at - listener;
            if (flat) d.z = 0.0f;
            const float dist = length(d);
            const float near = std::max(0.0f, v.cmd.min_distance);
            const float range = std::max(near + 0.01f, v.cmd.range);
            float fall = dist <= near ? 1.0f : std::clamp(1.0f - (dist - near) / (range - near), 0.0f, 1.0f);
            fall *= fall;
            gain *= fall;
            if (dist > 1e-3f) pan = std::clamp(dot(d * (1.0f / dist), r), -1.0f, 1.0f) * 0.8f;
        }
        m->mixer->set_gain(v.id, gain);
        m->mixer->set_pan(v.id, pan);
        ++it;
    }
}

void SoundPlayer::stop_all() {
    if (m->mixer) m->mixer->stop_all(0.05);
    m->voices.clear();
}

void SoundPlayer::preview(const std::string& clip, float volume) {
    if (!m->ready(m_error)) return;
    const fa::Sound s = m->load(resolve(clip), m_error);
    if (!s) return;
    if (m->preview) m->mixer->stop(m->preview, 0.02);
    fa::PlayOptions o;
    o.gain = volume;
    m->preview = m->mixer->play(s, o);
}

std::size_t SoundPlayer::playing() const {
    return m->voices.size();
}

} // namespace editor
} // namespace orchard
