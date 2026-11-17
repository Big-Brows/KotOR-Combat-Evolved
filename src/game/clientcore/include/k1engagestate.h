#pragma once

// Input/mode policy only. Engine adapters own actors, animations, pause and damage.
struct K1EngageState {
    enum class Mode { Exploration, Planning, Active };
    Mode mode = Mode::Exploration;
    bool rightHeld = false;
    bool leftHeld = false;
    bool holdChain = false;
    bool attackRequested = false;
    bool attacking = false;
    int chainSwings = 0;

    void ClearInputs() {
        rightHeld = leftHeld = holdChain = attackRequested = attacking = false;
        chainSwings = 0;
    }
    bool DetectCombat() {
        if (mode != Mode::Exploration) return false;
        mode = Mode::Planning; ClearInputs(); return true;
    }
    bool Engage(bool allowed) {
        if (!allowed || mode == Mode::Active) return false;
        mode = Mode::Active; ClearInputs(); return true;
    }
    void Plan() {
        if (mode == Mode::Exploration) return;
        mode = Mode::Planning; ClearInputs();
    }
    bool Disengage(bool spotted) {
        if (spotted) return false;
        mode = Mode::Exploration; ClearInputs(); return true;
    }
    bool Mouse(bool left, bool down) {
        if (mode != Mode::Active) return false;
        if (left) {
            leftHeld = down;
            if(!down) holdChain = false;
        }
        else rightHeld = down;
        return true;
    }
    bool ForceWalk() const { return mode == Mode::Active && rightHeld; }
    bool Guarding() const { return ForceWalk() && !attacking; }
    static int AttackDirection(float forward, float side) {
        // Exchange left and right saber clips while leaving movement input alone.
        if (forward > .25f) return side > 0 ? 5 : 6;
        if (forward < -.25f && (side < -.25f || side > .25f)) return side < 0 ? 2 : 1;
        if (side < -.25f) return 4;
        if (side > .25f) return 3;
        return 0;
    }
};
