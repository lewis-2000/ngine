#include "Animation.h"
#include "Robot.h"

#include <cassert>
#include <cmath>
#include <string>

namespace
{
    Mara::AnimationClip clip(
        const char *name,
        Mara::AnimationClip::Layer layer,
        int priority,
        bool loop = false)
    {
        Mara::AnimationClip result;
        result.name = name;
        result.duration = 1.0f;
        result.layer = layer;
        result.priority = priority;
        result.loop = loop;
        result.tracks.push_back(
            {"test_joint", {{0.0f, 0.0f}, {1.0f, 1.0f}}});
        return result;
    }
}

int main()
{
    using namespace Mara;

    AnimationEventQueue events;
    events.push({AnimationEventType::Idle, 1, "test"});
    events.push({AnimationEventType::Alert, 10, "test"});
    AnimationEvent event;
    assert(events.tryPop(event));
    assert(event.type == AnimationEventType::Alert);
    assert(events.tryPop(event));
    assert(event.type == AnimationEventType::Idle);

    AnimationCatalog catalog;
    catalog.registerClip(clip("idle", AnimationClip::Layer::Base, 1, true));
    catalog.registerClip(clip("greeting", AnimationClip::Layer::UpperBody, 10));
    AnimationSelector selector;
    const AnimationClip *selected =
        selector.select(catalog, {"greeting", 0, false});
    assert(selected && selected->name == "greeting");

    AnimationEventMapper mapper;
    AnimationRequest request;
    assert(mapper.parseIntentJson(
        R"({"intent":"greeting","priority":10,"interrupt":true})",
        request));
    assert(request.intent == "greeting");
    assert(request.priority == 10);
    assert(request.interruptCurrent);

    Robot robot;
    AnimationPlayer player;
    AnimationClip timed = clip("timed", AnimationClip::Layer::Base, 0);
    player.setClip(timed);
    player.update(0.5f, robot);
    assert(std::abs(player.time() - 0.5f) < 0.001f);
    assert(!player.finished());
    player.update(0.6f, robot);
    assert(player.finished());

    AnimationMixer mixer;
    AnimationClip idle = clip("idle", AnimationClip::Layer::Base, 1, true);
    mixer.setIdleClip(idle);
    AnimationClip greeting =
        clip("greeting", AnimationClip::Layer::UpperBody, 10);
    greeting.returnToIdle = true;
    mixer.setClip(idle, robot);
    mixer.setClip(greeting, robot);
    assert(mixer.currentAnimation() == "greeting");
    mixer.update(1.1f, robot);
    assert(mixer.currentAnimation() == "idle");

    return 0;
}
