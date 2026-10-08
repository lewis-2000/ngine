#include "Animation.h"

#include "Robot.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <regex>
#include <sstream>
#include <utility>

namespace Mara
{
    namespace
    {
        std::string readText(const std::filesystem::path &path)
        {
            std::ifstream file(path);
            if (!file)
                return {};
            std::ostringstream content;
            content << file.rdbuf();
            return content.str();
        }

        std::string jsonString(
            const std::string &text,
            const char *key,
            const std::string &fallback)
        {
            const std::regex pattern(
                std::string("\"") + key + "\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch match;
            return std::regex_search(text, match, pattern)
                       ? match[1].str()
                       : fallback;
        }

        float jsonFloat(
            const std::string &text,
            const char *key,
            float fallback)
        {
            const std::regex pattern(
                std::string("\"") + key + R"("\s*:\s*(-?[0-9]+(?:\.[0-9]+)?))");
            std::smatch match;
            return std::regex_search(text, match, pattern)
                       ? std::stof(match[1].str())
                       : fallback;
        }

        int jsonInt(const std::string &text, const char *key, int fallback)
        {
            return static_cast<int>(jsonFloat(text, key, static_cast<float>(fallback)));
        }

        bool jsonBool(
            const std::string &text,
            const char *key,
            bool fallback)
        {
            const std::regex pattern(
                std::string("\"") + key + R"("\s*:\s*(true|false))");
            std::smatch match;
            if (!std::regex_search(text, match, pattern))
                return fallback;
            return match[1].str() == "true";
        }

        AnimationClip::Layer jsonLayer(
            const std::string &text,
            AnimationClip::Layer fallback)
        {
            const std::string value = jsonString(text, "layer", "");
            if (value == "upper_body")
                return AnimationClip::Layer::UpperBody;
            if (value == "head")
                return AnimationClip::Layer::Head;
            if (value == "emergency")
                return AnimationClip::Layer::Emergency;
            return value == "base" ? AnimationClip::Layer::Base : fallback;
        }
    }

    bool AnimationSafety::allows(
        const AnimationClip &clip,
        const AnimationSafetyState &state)
    {
        return rejectionReason(clip, state)[0] == '\0';
    }

    const char *AnimationSafety::rejectionReason(
        const AnimationClip &clip,
        const AnimationSafetyState &state)
    {
        if (state.emergencyStop)
            return "emergency stop is active";
        if (!state.motorsEnabled)
            return "motors are disabled";
        if (state.harnessAttached &&
            (!clip.allowsHarness || clip.requiresGroundContact))
            return "clip is unsafe while the harness is attached";
        if (clip.requiresGroundContact && !state.groundContact)
            return "clip requires ground contact";
        return "";
    }

    AnimationRequest AnimationEventMapper::toRequest(
        const AnimationEvent &event) const
    {
        AnimationRequest request;
        request.priority = event.priority;
        request.interruptCurrent = event.priority >= 10;
        switch (event.type)
        {
        case AnimationEventType::Startup:
            request.intent = "startup";
            break;
        case AnimationEventType::Idle:
            request.intent = "idle";
            break;
        case AnimationEventType::Greeting:
            request.intent = "greeting";
            break;
        case AnimationEventType::Alert:
            request.intent = "alert";
            break;
        case AnimationEventType::Error:
            request.intent = "error";
            break;
        }
        return request;
    }

    bool AnimationEventMapper::parseIntentJson(
        const std::string &json,
        AnimationRequest &request) const
    {
        const std::regex intentPattern(
            "\"intent\"\\s*:\\s*\"([a-zA-Z0-9_-]+)\"");
        const std::regex priorityPattern(
            "\"priority\"\\s*:\\s*(-?[0-9]+)");
        const std::regex interruptPattern(
            "\"interrupt\"\\s*:\\s*(true|false)");
        std::smatch match;
        if (!std::regex_search(json, match, intentPattern))
            return false;

        request = {};
        request.intent = match[1].str();
        if (std::regex_search(json, match, priorityPattern))
            request.priority = std::stoi(match[1].str());
        if (std::regex_search(json, match, interruptPattern))
            request.interruptCurrent = match[1].str() == "true";
        return true;
    }

    void AnimationEventQueue::push(AnimationEvent event)
    {
        const auto position = std::find_if(
            m_Events.begin(),
            m_Events.end(),
            [&event](const AnimationEvent &queued)
            {
                return event.priority > queued.priority;
            });
        m_Events.insert(position, std::move(event));
    }

    bool AnimationEventQueue::tryPop(AnimationEvent &event)
    {
        if (m_Events.empty())
            return false;

        event = std::move(m_Events.front());
        m_Events.pop_front();
        return true;
    }

    void AnimationCatalog::registerClip(AnimationClip clip)
    {
        const auto existing = m_ClipIndices.find(clip.name);
        if (existing != m_ClipIndices.end())
        {
            m_Clips[existing->second] = std::move(clip);
            return;
        }

        m_ClipIndices.emplace(clip.name, m_Clips.size());
        m_Clips.push_back(std::move(clip));
    }

    const AnimationClip *AnimationCatalog::find(const std::string &name) const
    {
        const auto it = m_ClipIndices.find(name);
        return it == m_ClipIndices.end() ? nullptr : &m_Clips[it->second];
    }

    const AnimationClip *AnimationSelector::select(
        const AnimationCatalog &catalog,
        const AnimationRequest &request) const
    {
        const AnimationClip *selected = nullptr;
        for (const AnimationClip &clip : catalog.clips())
        {
            const bool matchesIntent =
                clip.name == request.intent ||
                std::find(clip.tags.begin(), clip.tags.end(), request.intent) !=
                    clip.tags.end();
            if (!matchesIntent || clip.priority < request.priority)
                continue;

            if (!selected || clip.priority > selected->priority)
                selected = &clip;
        }
        return selected;
    }

    void AnimationLibrary::rebuild(
        const Robot &robot,
        float duration,
        bool loop,
        bool selectedOnly,
        const std::string &selectedLink)
    {
        m_Catalog = {};
        constexpr int sampleCount = 32;
        const auto buildClip = [&](const char *name,
                                   std::initializer_list<const char *> tags,
                                   int priority,
                                   float motionScale,
                                   AnimationClip::Layer layer)
        {
            AnimationClip clip;
            clip.name = name;
            clip.duration = duration;
            clip.priority = priority;
            clip.loop = loop;
            clip.layer = layer;
            clip.returnToIdle = layer != AnimationClip::Layer::Base;
            clip.requiresGroundContact = false;
            clip.allowsHarness = true;
            for (const char *tag : tags)
                clip.tags.emplace_back(tag);

            const auto addTrack = [&clip, motionScale, duration](
                                      const UrdfJoint &joint)
            {
                if (joint.type == UrdfJoint::Type::FIXED)
                    return;
                const bool upperBody =
                    joint.name.find("joint") != std::string::npos &&
                    (joint.name.find("left_") == 0 ||
                     joint.name.find("right_") == 0 ||
                     joint.name.find("shoulder_") == 0 ||
                     joint.name.find("elbow_") == 0);
                if (clip.layer == AnimationClip::Layer::UpperBody && !upperBody)
                    return;

                float amplitude = 0.5f;
                float center = 0.0f;
                const bool locked =
                    joint.name == "hip_yaw_l_joint" ||
                    joint.name == "hip_yaw_r_joint" ||
                    joint.name == "hip_roll_l_joint" ||
                    joint.name == "hip_roll_r_joint" ||
                    joint.name == "left_joint1" ||
                    joint.name == "right_joint1" ||
                    joint.name == "left_joint3" ||
                    joint.name == "right_joint3";
                if (joint.limit.has_limit)
                {
                    const float lower = static_cast<float>(joint.limit.lower);
                    const float upper = static_cast<float>(joint.limit.upper);
                    center = (lower + upper) * 0.5f;
                    amplitude = (upper - lower) * 0.5f;
                }
                if (locked)
                {
                    center = 0.0f;
                    amplitude = 0.0f;
                }

                JointAnimationTrack track;
                track.jointName = joint.name;
                track.keyframes.reserve(sampleCount + 1);
                for (int sample = 0; sample <= sampleCount; ++sample)
                {
                    const float time = duration * sample / sampleCount;
                    track.keyframes.push_back(
                        {time, center + amplitude * motionScale * std::sin(time)});
                }
                clip.tracks.push_back(std::move(track));
            };

            if (selectedOnly)
            {
                if (const UrdfJoint *joint = robot.jointForChildLink(selectedLink))
                    addTrack(*joint);
            }
            else
            {
                for (const auto &[jointName, joint] : robot.data().joints)
                    addTrack(joint);
            }
            m_Catalog.registerClip(std::move(clip));
        };

        buildClip("demo_wave", {"demo"}, 0, 1.0f, AnimationClip::Layer::Base);
        buildClip("idle", {"idle"}, 1, 0.15f, AnimationClip::Layer::Base);
        buildClip("greeting", {"greeting"}, 10, 1.0f, AnimationClip::Layer::UpperBody);
        loadFromDirectory(
            std::filesystem::path("resources") / "animations",
            robot,
            selectedOnly,
            selectedLink);
    }

    bool AnimationLibrary::loadFromDirectory(
        const std::filesystem::path &directory,
        const Robot &robot,
        bool selectedOnly,
        const std::string &selectedLink)
    {
        if (!std::filesystem::exists(directory))
            return false;

        bool loaded = false;
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            const std::string text = readText(entry.path());
            const std::string name =
                jsonString(text, "name", entry.path().stem().string());
            AnimationClip clip;
            clip.name = name;
            clip.duration = jsonFloat(text, "duration", 1.0f);
            clip.priority = jsonInt(text, "priority", 0);
            clip.loop = jsonBool(text, "loop", false);
            clip.returnToIdle = jsonBool(text, "return_to_idle", false);
            clip.requiresGroundContact =
                jsonBool(text, "requires_ground_contact", false);
            clip.allowsHarness = jsonBool(text, "allows_harness", true);
            clip.layer = jsonLayer(text, AnimationClip::Layer::Base);
            const float motionScale = jsonFloat(text, "motion_scale", 1.0f);
            clip.tags.push_back(name);

            const auto addTrack = [&clip, motionScale](
                                      const UrdfJoint &joint)
            {
                if (joint.type == UrdfJoint::Type::FIXED)
                    return;
                const bool upperBody =
                    joint.name.find("joint") != std::string::npos &&
                    (joint.name.find("left_") == 0 ||
                     joint.name.find("right_") == 0 ||
                     joint.name.find("shoulder_") == 0 ||
                     joint.name.find("elbow_") == 0);
                if (clip.layer == AnimationClip::Layer::UpperBody && !upperBody)
                    return;
                float amplitude = 0.5f;
                float center = 0.0f;
                const bool locked =
                    joint.name == "hip_yaw_l_joint" ||
                    joint.name == "hip_yaw_r_joint" ||
                    joint.name == "hip_roll_l_joint" ||
                    joint.name == "hip_roll_r_joint" ||
                    joint.name == "left_joint1" ||
                    joint.name == "right_joint1" ||
                    joint.name == "left_joint3" ||
                    joint.name == "right_joint3";
                if (joint.limit.has_limit)
                {
                    const float lower = static_cast<float>(joint.limit.lower);
                    const float upper = static_cast<float>(joint.limit.upper);
                    center = (lower + upper) * 0.5f;
                    amplitude = (upper - lower) * 0.5f;
                }
                if (locked)
                {
                    center = 0.0f;
                    amplitude = 0.0f;
                }
                clip.tracks.push_back(
                    {joint.name,
                     {{0.0f, center},
                      {clip.duration, center + amplitude * motionScale}}});
            };

            if (selectedOnly)
            {
                if (const UrdfJoint *joint = robot.jointForChildLink(selectedLink))
                    addTrack(*joint);
            }
            else
            {
                for (const auto &[jointName, joint] : robot.data().joints)
                    addTrack(joint);
            }
            m_Catalog.registerClip(std::move(clip));
            loaded = true;
        }
        return loaded;
    }

    void AnimationPlayer::setClip(AnimationClip clip)
    {
        m_Clip = std::move(clip);
        m_Time = 0.0f;
        m_BlendStartPositions.clear();
        m_BlendTime = 0.0f;
        m_BlendDuration = 0.0f;
    }

    void AnimationPlayer::setClip(
        AnimationClip clip,
        Robot &robot,
        float blendDuration)
    {
        m_BlendStartPositions.clear();
        for (const JointAnimationTrack &track : clip.tracks)
            m_BlendStartPositions.emplace(
                track.jointName,
                robot.jointPosition(track.jointName));

        m_Clip = std::move(clip);
        m_Time = 0.0f;
        m_BlendTime = 0.0f;
        m_BlendDuration = std::max(0.0f, blendDuration);
    }

    void AnimationPlayer::clear()
    {
        m_Clip = {};
        m_Time = 0.0f;
        m_BlendStartPositions.clear();
        m_BlendTime = 0.0f;
        m_BlendDuration = 0.0f;
    }

    void AnimationPlayer::update(float deltaTime, Robot &robot)
    {
        if (!hasClip())
            return;

        const float step = std::max(0.0f, deltaTime);
        if (m_Clip.loop)
        {
            m_Time = std::fmod(m_Time + step, m_Clip.duration);
            if (m_Time < 0.0f)
                m_Time += m_Clip.duration;
        }
        else
        {
            m_Time = std::clamp(m_Time + step, 0.0f, m_Clip.duration);
        }
        m_BlendTime += step;
        apply(robot);
    }

    void AnimationPlayer::seek(float time, Robot &robot)
    {
        if (!hasClip())
            return;

        m_BlendStartPositions.clear();
        m_BlendTime = 0.0f;
        m_BlendDuration = 0.0f;
        if (m_Clip.loop)
        {
            m_Time = std::fmod(std::max(0.0f, time), m_Clip.duration);
            if (m_Time < 0.0f)
                m_Time += m_Clip.duration;
        }
        else
        {
            m_Time = std::clamp(time, 0.0f, m_Clip.duration);
        }

        apply(robot);
    }

    void AnimationPlayer::apply(Robot &robot) const
    {
        for (const JointAnimationTrack &track : m_Clip.tracks)
        {
            const float target = valueAt(track, m_Time);
            const auto start = m_BlendStartPositions.find(track.jointName);
            float position = target;
            if (start != m_BlendStartPositions.end() &&
                m_BlendDuration > 0.0f &&
                m_BlendTime < m_BlendDuration)
            {
                const float factor = std::clamp(
                    m_BlendTime / m_BlendDuration,
                    0.0f,
                    1.0f);
                position = start->second + (target - start->second) * factor;
            }
            robot.setJointPosition(track.jointName, position);
        }
    }

    float AnimationPlayer::valueAt(
        const JointAnimationTrack &track,
        float time) const
    {
        if (track.keyframes.empty())
            return 0.0f;
        if (track.keyframes.size() == 1 || time <= track.keyframes.front().time)
            return track.keyframes.front().position;
        if (time >= track.keyframes.back().time)
            return track.keyframes.back().position;

        const auto next = std::upper_bound(
            track.keyframes.begin(),
            track.keyframes.end(),
            time,
            [](float value, const JointKeyframe &keyframe)
            {
                return value < keyframe.time;
            });
        const JointKeyframe &end = *next;
        const JointKeyframe &start = *(next - 1);
        const float span = end.time - start.time;
        const float factor = span > 0.0f ? (time - start.time) / span : 0.0f;
        return start.position + (end.position - start.position) * factor;
    }

    void AnimationMixer::setClip(
        AnimationClip clip,
        Robot &robot,
        float blendDuration)
    {
        const std::size_t index = static_cast<std::size_t>(clip.layer);
        if (index >= LayerCount)
            return;
        m_Layers[index].setClip(std::move(clip), robot, blendDuration);
        refreshState();
    }

    void AnimationMixer::update(float deltaTime, Robot &robot)
    {
        bool returnToIdle = false;
        for (AnimationPlayer &layer : m_Layers)
        {
            layer.update(deltaTime, robot);
            if (layer.finished())
            {
                returnToIdle = returnToIdle || layer.returnsToIdle();
                layer.clear();
            }
        }
        if (returnToIdle && m_IdleClip.duration > 0.0f)
        {
            const std::size_t baseIndex =
                static_cast<std::size_t>(AnimationClip::Layer::Base);
            m_Layers[baseIndex].setClip(m_IdleClip, robot);
        }
        refreshState();
    }

    void AnimationMixer::setIdleClip(AnimationClip clip)
    {
        clip.layer = AnimationClip::Layer::Base;
        m_IdleClip = std::move(clip);
    }

    void AnimationMixer::seek(float time, Robot &robot)
    {
        for (AnimationPlayer &layer : m_Layers)
            layer.seek(time, robot);
        refreshState();
    }

    bool AnimationMixer::hasClip() const
    {
        for (const AnimationPlayer &layer : m_Layers)
            if (layer.hasClip())
                return true;
        return false;
    }

    int AnimationMixer::priority() const
    {
        return m_CurrentPriority;
    }

    float AnimationMixer::time() const
    {
        const AnimationPlayer *active = activePlayer();
        return active ? active->time() : 0.0f;
    }

    float AnimationMixer::duration() const
    {
        const AnimationPlayer *active = activePlayer();
        return active ? active->duration() : 0.0f;
    }

    const std::string &AnimationMixer::currentAnimation() const
    {
        return m_CurrentAnimation;
    }

    void AnimationMixer::refreshState()
    {
        const AnimationPlayer *active = activePlayer();
        if (!active)
        {
            m_CurrentAnimation.clear();
            m_CurrentPriority = 0;
            return;
        }

        m_CurrentAnimation = active->currentAnimation();
        m_CurrentPriority = active->priority();
    }

    const AnimationPlayer *AnimationMixer::activePlayer() const
    {
        const AnimationPlayer *active = nullptr;
        for (const AnimationPlayer &layer : m_Layers)
        {
            if (!layer.hasClip())
                continue;
            if (!active || layer.priority() >= active->priority())
                active = &layer;
        }
        return active;
    }
}
