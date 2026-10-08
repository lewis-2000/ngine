#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <deque>
#include <unordered_map>
#include <vector>

class Robot;

namespace Mara
{
    struct JointKeyframe
    {
        float time = 0.0f;
        float position = 0.0f;
    };

    struct JointAnimationTrack
    {
        std::string jointName;
        std::vector<JointKeyframe> keyframes;
    };

    struct AnimationClip
    {
        enum class Layer
        {
            Base,
            UpperBody,
            Head,
            Emergency
        };

        std::string name;
        std::vector<std::string> tags;
        float duration = 0.0f;
        int priority = 0;
        bool loop = false;
        Layer layer = Layer::Base;
        bool returnToIdle = false;
        bool requiresGroundContact = false;
        bool allowsHarness = true;
        std::vector<JointAnimationTrack> tracks;
    };

    struct AnimationRequest
    {
        std::string intent;
        int priority = 0;
        bool interruptCurrent = false;
    };

    struct AnimationSafetyState
    {
        bool harnessAttached = true;
        bool groundContact = false;
        bool emergencyStop = false;
        bool motorsEnabled = true;
    };

    class AnimationSafety
    {
    public:
        static bool allows(
            const AnimationClip &clip,
            const AnimationSafetyState &state);
        static const char *rejectionReason(
            const AnimationClip &clip,
            const AnimationSafetyState &state);
    };

    enum class AnimationEventType
    {
        Startup,
        Idle,
        Greeting,
        Alert,
        Error
    };

    struct AnimationEvent
    {
        AnimationEventType type = AnimationEventType::Idle;
        int priority = 0;
        std::string source;
    };

    class AnimationEventMapper
    {
    public:
        AnimationRequest toRequest(const AnimationEvent &event) const;
        bool parseIntentJson(
            const std::string &json,
            AnimationRequest &request) const;
    };

    class AnimationEventQueue
    {
    public:
        void push(AnimationEvent event);
        bool tryPop(AnimationEvent &event);
        bool empty() const { return m_Events.empty(); }
        std::size_t size() const { return m_Events.size(); }
        void clear() { m_Events.clear(); }

    private:
        std::deque<AnimationEvent> m_Events;
    };

    class AnimationCatalog
    {
    public:
        void registerClip(AnimationClip clip);
        const AnimationClip *find(const std::string &name) const;
        const std::vector<AnimationClip> &clips() const { return m_Clips; }

    private:
        std::vector<AnimationClip> m_Clips;
        std::unordered_map<std::string, std::size_t> m_ClipIndices;
    };

    class AnimationSelector
    {
    public:
        const AnimationClip *select(
            const AnimationCatalog &catalog,
            const AnimationRequest &request) const;
    };

    class AnimationLibrary
    {
    public:
        void rebuild(
            const Robot &robot,
            float duration,
            bool loop,
            bool selectedOnly,
            const std::string &selectedLink);
        bool loadFromDirectory(
            const std::filesystem::path &directory,
            const Robot &robot,
            bool selectedOnly,
            const std::string &selectedLink);
        const AnimationCatalog &catalog() const { return m_Catalog; }

    private:
        AnimationCatalog m_Catalog;
    };

    class AnimationPlayer
    {
    public:
        void setClip(AnimationClip clip);
        void setClip(
            AnimationClip clip,
            Robot &robot,
            float blendDuration = 0.25f);
        void clear();
        void update(float deltaTime, Robot &robot);
        void seek(float time, Robot &robot);

        bool hasClip() const { return m_Clip.duration > 0.0f; }
        float time() const { return m_Time; }
        float duration() const { return m_Clip.duration; }
        const std::string &currentAnimation() const { return m_Clip.name; }
        int priority() const { return m_Clip.priority; }
        AnimationClip::Layer layer() const { return m_Clip.layer; }
        bool returnsToIdle() const { return m_Clip.returnToIdle; }
        bool finished() const
        {
            return hasClip() && !m_Clip.loop && m_Time >= m_Clip.duration;
        }

    private:
        void apply(Robot &robot) const;
        float valueAt(const JointAnimationTrack &track, float time) const;

        AnimationClip m_Clip;
        float m_Time = 0.0f;
        std::unordered_map<std::string, float> m_BlendStartPositions;
        float m_BlendTime = 0.0f;
        float m_BlendDuration = 0.0f;
    };

    class AnimationMixer
    {
    public:
        void setClip(AnimationClip clip, Robot &robot, float blendDuration = 0.25f);
        void setIdleClip(AnimationClip clip);
        void update(float deltaTime, Robot &robot);
        void seek(float time, Robot &robot);
        bool hasClip() const;
        int priority() const;
        float time() const;
        float duration() const;
        const std::string &currentAnimation() const;

    private:
        void refreshState();
        const AnimationPlayer *activePlayer() const;

        static constexpr std::size_t LayerCount = 4;
        std::array<AnimationPlayer, LayerCount> m_Layers;
        AnimationClip m_IdleClip;
        std::string m_CurrentAnimation;
        int m_CurrentPriority = 0;
    };
}
