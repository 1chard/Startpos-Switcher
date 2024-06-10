#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/StartPosObject.hpp>
#include <Geode/modify/UILayer.hpp>

using namespace geode::prelude;

// TODO map offset for others devices, offset-less mode is not performant 
#ifdef GEODE_IS_WINDOWS
#define HAS_STARTPOS_OFFSET
int offset = 0xB85;
#endif

std::vector<StartPosObject*> startPos = {};
int selectedStartpos = 0;
bool levelStarted = false;
bool shouldDefaultStartpos = true;
bool controllerOn = false;

CCLabelBMFont* label;
CCMenu* menu;
CCMenuItemSpriteExtra* leftBtn;
CCMenuItemSpriteExtra* rightBtn;

void updateLabel() {
    std::stringstream ss;
    ss << selectedStartpos + 1;
    ss << "/";
    ss << startPos.size();

    label->setString(ss.str().c_str());
}

auto newSprite(const char* name) {
    auto sprite = CCSprite::createWithSpriteFrameName(name);
    sprite->setOpacity(100);
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

void switchToStartpos(int incBy) {
    if (!levelStarted)
        return;

    shouldDefaultStartpos = false;

    selectedStartpos += incBy;

    if (selectedStartpos < -1)
        selectedStartpos = startPos.size() - 1;


    if (selectedStartpos >= startPos.size())
        selectedStartpos = -1;

    StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];

    auto playLayer = PlayLayer::get();

#ifdef HAS_STARTPOS_OFFSET
    int* startPosCheckpoint = (int*)playLayer + offset;
    *startPosCheckpoint = 0;
#endif

    playLayer->m_isTestMode = startPosObject != nullptr;

    playLayer->setStartPosObject(startPosObject);
    playLayer->resetLevel();

#ifdef HAS_STARTPOS_OFFSET
    playLayer->startMusic();
#endif

    updateLabel();

#ifndef GEODE_IS_ANDROID // ignore controller on android

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
}

class StartposSwitcher
{
public:
    void onLeft(CCObject*) {
        switchToStartpos(-1);
    }

    void onRight(CCObject*) {
        switchToStartpos(1);
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
        switchToStartpos(-1);
    }

    if (right) {
        switchToStartpos(1);
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

void clearData() {
    startPos.clear();
    selectedStartpos = -1;
    levelStarted = false;
    shouldDefaultStartpos = true;
}

void initSwitcher(PlayLayer* res) {
    if (startPos.size() == 0)
        menu->setVisible(false);
    else {
        std::sort(startPos.begin(), startPos.end(), [](StartPosObject* a1, StartPosObject* a2)
            { return a1->getPositionX() < a2->getPositionX(); });


        float highestPositionX = 0.f;

        for (size_t i = 0; i < startPos.size(); i++) {
            auto start = startPos[i];

            if (!start->m_startSettings->m_disableStartPos && start->getPositionX() > highestPositionX) {
                selectedStartpos = i;
                highestPositionX = start->getPositionX();
            }
        }

#ifndef HAS_STARTPOS_OFFSET // necessary for offset-less else the game will loop into a single startpos
        res->setStartPosObject(nullptr);
#endif

        updateLabel();
    }
}

class $modify(PlayLayer) {
#ifndef GEODE_IS_MACOS // PlayLayer::create isn't called on mac for some reason
    static PlayLayer* create(GJGameLevel * p0, bool p1, bool p2) {
        clearData();

        auto res = PlayLayer::create(p0, p1, p2);

        initSwitcher(res);

        return res;
    }
#else
    bool init(GJGameLevel * p0, bool p1, bool p2) {
        if (!PlayLayer::init(p0, p1, p2)) return false;

        initSwitcher(PlayLayer::get());

        return true;
    }
    void onQuit() {
        clearData();
        PlayLayer::onQuit();
    }
#endif

    void delayedResetLevel() {
#ifndef HAS_STARTPOS_OFFSET // we put current startpos here (on death) instead of init() so it does not loop
        if (shouldDefaultStartpos) {
            log::info("startpos delayedResetLevel");
            StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];
            PlayLayer::setStartPosObject(startPosObject);
        }
#endif

        PlayLayer::delayedResetLevel();
    }

    void levelComplete() {
#ifndef HAS_STARTPOS_OFFSET // we put current startpos here (on success) instead of init() so it does not loop
        if (shouldDefaultStartpos) {
            log::info("startpos levelComplete");
            StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];
            PlayLayer::setStartPosObject(startPosObject);
        }
#endif

        PlayLayer::levelComplete();
    }

    void startGame() {// this function trigger when player start to move at attempt 1
        if (!levelStarted) // unlock switcher
            levelStarted = true;

        PlayLayer::startGame();
    }

#ifndef HAS_STARTPOS_OFFSET // unloop music on mobile
    void resetLevel() {
        PlayLayer::resetLevel();
        PlayLayer::prepareMusic(false);
    }
#endif
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
        label->setOpacity(100);

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
#ifdef GEODE_IS_MACOS
    static StartPosObject* create() {
        auto res = StartPosObject::create();

        if (auto plr = PlayLayer::get()) {
            startPos.push_back(static_cast<StartPosObject*>(res));
        }

        return res;
    }
#else
    virtual bool init() {
        if (!StartPosObject::init())
            return false;

        if (auto plr = PlayLayer::get()) {
            startPos.push_back(static_cast<StartPosObject*>(this));
        }

        return true;
    }
#endif
};
