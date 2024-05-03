#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/StartPosObject.hpp>
#include <Geode/modify/UILayer.hpp>

using namespace geode::prelude;

std::vector<StartPosObject*> startPos = {};
int selectedStartpos = 0;
bool levelStarted = false;
bool shouldDefaultStartpos = true;
bool controllerOn = false;

CCLabelBMFont* label;
CCMenu* menu;
CCMenuItemSpriteExtra* leftBtn;
CCMenuItemSpriteExtra* rightBtn;

$execute
{
#ifdef GEODE_IS_ANDROID64
    // 1F2003D5
    Mod::get()->patch(reinterpret_cast<void*>(geode::base::get() + 0x82803c), {0x1F, 0x20, 0x03, 0xD5});
#endif
}

void updateLabel() {
    std::stringstream ss;
    ss << selectedStartpos + 1;
    ss << "/";
    ss << startPos.size();

    label->setString(ss.str().c_str());
}

void switchToStartpos(int incBy) {
    if (!levelStarted)
        return;

    shouldDefaultStartpos = false;

    selectedStartpos += incBy;

    if (selectedStartpos < -1)
        selectedStartpos = startPos.size() - 1;

    if (selectedStartpos >= startPos.size())
        selectedStartpos = -1;

    updateLabel();

    log::info("startpos: {}", selectedStartpos);

    StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];

    auto playLayer = PlayLayer::get();

    playLayer->setStartPosObject(startPosObject);
    playLayer->fullReset();
}

auto newSprite(const char* name) {
    auto sprite = CCSprite::createWithSpriteFrameName(name);
    sprite->setOpacity(Mod::get()->getSettingValue<int64_t>("ui-opacity"));
    return sprite;
}

auto spriteNormalLeft() {
    return newSprite("GJ_arrow_02_001.png");
}

auto spriteNormalRight() {
    auto sprite = spriteNormalLeft();
    sprite->setFlipX(true);
    return sprite;
}

#ifndef GEODE_IS_ANDROID

auto spriteControllerLeft() {
    return newSprite("controllerBtn_DPad_Left_001.png");
}

auto spriteControllerRight() {
    return newSprite("controllerBtn_DPad_Right_001.png");
}

#endif

void onMove(int incBy) {
#ifndef GEODE_IS_ANDROID // ignore extra args on mobile

    if (controllerOn != PlatformToolbox::isControllerConnected()) {
        controllerOn = !controllerOn;

        CCSprite* leftSprTarget;
        CCSprite* rightSprTarget;

        if (controllerOn) {
            leftSprTarget = spriteControllerLeft();
            rightSprTarget = spriteControllerRight();
        }
        else {
            leftSprTarget = spriteNormalLeft();
            rightSprTarget = spriteNormalRight();
        }

        leftBtn->setSprite(leftSprTarget);
        leftBtn->setContentWidth(50);
        leftBtn->setContentHeight(50);

        rightBtn->setSprite(rightSprTarget);
        rightBtn->setContentWidth(50);
        rightBtn->setContentHeight(50);

        leftSprTarget->setPosition(leftBtn->getContentSize() / 2);
        rightSprTarget->setPosition(rightBtn->getContentSize() / 2);
    }

#endif

    switchToStartpos(incBy);
}

class StartposSwitcher
{
public:
    void onLeft(CCObject*) {
        onMove(-1);
    }

    void onRight(CCObject*) {
        onMove(1);
    }
};

#ifndef GEODE_IS_ANDROID

void onDown(enumKeyCodes key) {
    if (startPos.size() == 0)
        return;

    std::vector<enumKeyCodes> leftKeys = {
        enumKeyCodes::KEY_Subtract,
        enumKeyCodes::KEY_OEMComma,
        enumKeyCodes::CONTROLLER_Left,
    };

    std::vector<enumKeyCodes> rightKeys = {
        enumKeyCodes::KEY_Add,
        enumKeyCodes::KEY_OEMPeriod,
        enumKeyCodes::CONTROLLER_Right,
    };

    if (!PlayLayer::get()->m_player1->m_isPlatformer) {
        // leftKeys.push_back(enumKeyCodes::KEY_W);
        leftKeys.push_back(enumKeyCodes::KEY_Left);

        // rightKeys.push_back(enumKeyCodes::KEY_D);
        rightKeys.push_back(enumKeyCodes::KEY_Right);
    }

    if (!PlayLayer::get()->m_isPracticeMode) {
        leftKeys.push_back(enumKeyCodes::KEY_Z);

        rightKeys.push_back(enumKeyCodes::KEY_X);
    }

    bool left = false;
    bool right = false;

    for (size_t i = 0; i < leftKeys.size(); i++) {
        if (key == leftKeys[i])
            left = true;
    }

    for (size_t i = 0; i < rightKeys.size(); i++) {
        if (key == rightKeys[i])
            right = true;
    }

    if (left) {
        onMove(-1);
    }

    if (right) {
        onMove(1);
    }
}

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool idk) {
        if (!CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, idk)) return false;

        if (PlayLayer::get() && down && !idk) {
            onDown(key);
        }

        return true;
    }
};

#endif

class $modify(PlayLayer) {
    static PlayLayer* create(GJGameLevel * p0, bool p1, bool p2) {
        startPos.clear();
        selectedStartpos = -1;
        levelStarted = false;
        shouldDefaultStartpos = true;

        auto res = PlayLayer::create(p0, p1, p2);

        if (startPos.size() == 0)
            menu->setVisible(false);
        else {
            if (Mod::get()->getSettingValue<bool>("sort-triggers")) {
                std::sort(startPos.begin(), startPos.end(), [](StartPosObject* a1, StartPosObject* a2)
                    { return a1->getPositionX() < a2->getPositionX(); });
            }

            float highestPositionX = 0.f;

            for (size_t i = 0; i < startPos.size(); i++) {
                auto start = startPos[i];

                if (!start->m_startSettings->m_disableStartPos && start->getPositionX() > highestPositionX) {
                    selectedStartpos = i;
                    highestPositionX = start->getPositionX();
                }
            }

            // necessary else the game will loop into a single startpos
            res->setStartPosObject(nullptr);

            updateLabel();
        }

        return res;
    }

    void delayedResetLevel() {
        if (shouldDefaultStartpos) { // we put current startpos here (on death) instead of init() so it does not loop
            StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];
            PlayLayer::setStartPosObject(startPosObject);
        }

        PlayLayer::delayedResetLevel();
    }

    void startGame() {// this function trigger when player start to move at attempt 1
        if (!levelStarted) // unlock switcher
            levelStarted = true;

        PlayLayer::startGame();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        PlayLayer::prepareMusic(false);
    }
};

class $modify(UILayer) {
    bool init(GJBaseGameLayer * idk) {
        if (!UILayer::init(idk)) return false;

        if (!typeinfo_cast<PlayLayer*>(idk))
            return true;

        menu = CCMenu::create();
        menu->setID("startpos-switcher");
        menu->setPosition(ccp(CCDirector::get()->getWinSize().width / 2, 25));
        menu->setContentSize(ccp(0, 0));
        menu->setScale(0.6f);
        menu->setVisible(Mod::get()->getSettingValue<bool>("show-ui"));

        label = CCLabelBMFont::create("0/0", "bigFont.fnt");
        label->setPosition(ccp(0, 0));
        label->setOpacity(Mod::get()->getSettingValue<int64_t>("ui-opacity"));

        CCSprite* leftSprTarget;
        CCSprite* rightSprTarget;

#ifndef GEODE_IS_ANDROID // only create controller sprites on pc
        controllerOn = PlatformToolbox::isControllerConnected();

        if (controllerOn) {
            leftSprTarget = spriteControllerLeft();
            rightSprTarget = spriteControllerRight();
        }
        else {
            leftSprTarget = spriteNormalLeft();
            rightSprTarget = spriteNormalRight();
        }
#else
        leftSprTarget = spriteNormalLeft();
        rightSprTarget = spriteNormalRight();
#endif

        leftBtn = CCMenuItemSpriteExtra::create(leftSprTarget, menu, menu_selector(StartposSwitcher::onLeft));
        leftBtn->setContentWidth(50);
        leftBtn->setContentHeight(50);
        leftBtn->setPosition(ccp(-85, 0));

        rightBtn = CCMenuItemSpriteExtra::create(rightSprTarget, menu, menu_selector(StartposSwitcher::onRight));
        rightBtn->setContentWidth(50);
        rightBtn->setContentHeight(50);
        rightBtn->setPosition(ccp(85, 0));

        leftSprTarget->setPosition(leftBtn->getContentSize() / 2);
        rightSprTarget->setPosition(rightBtn->getContentSize() / 2);

        menu->addChild(label);
        menu->addChild(leftBtn);
        menu->addChild(rightBtn);
        this->addChild(menu);
        return true;
    }
};

class $modify(StartPosObject) {
    virtual bool init() {
        if (!StartPosObject::init()) return false;

        if (auto plr = PlayLayer::get()) {
            startPos.push_back(static_cast<StartPosObject*>(this));
        }

        return true;
    }
};