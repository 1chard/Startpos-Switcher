#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/StartPosObject.hpp>
#include <Geode/modify/UILayer.hpp>

using namespace geode::prelude;

std::vector<StartPosObject*> startPos = {};
int selectedStartpos = -1;
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
    selectedStartpos += incBy;

    if (selectedStartpos < -1)
        selectedStartpos = startPos.size() - 1;


    if (selectedStartpos >= startPos.size())
        selectedStartpos = -1;

    StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];

    auto playLayer = PlayLayer::get();

    playLayer->m_currentCheckpoint = nullptr;

    playLayer->m_isTestMode = startPosObject != nullptr;

    playLayer->setStartPosObject(startPosObject);
    playLayer->resetLevel();
    playLayer->startMusic();

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

class $modify(PlayLayer) {
    // code from qolmod
    void addObject(GameObject * obj) {
        if (obj->m_objectID == 31) {
            startPos.push_back(as<StartPosObject*>(obj));
        }

        PlayLayer::addObject(obj);
    }

    bool init(GJGameLevel * p0, bool p1, bool p2) {
        if (!PlayLayer::init(p0, p1, p2)) return false;

        if (startPos.size() == 0)
            menu->setVisible(false);
        else {
            std::sort(startPos.begin(), startPos.end(), [](StartPosObject* a1, StartPosObject* a2)
                { return a1->m_realXPosition < a2->m_realXPosition; });

            double highestPositionX = 0.0;

            for (size_t i = 0; i < startPos.size(); i++) {
                auto start = startPos[i];

                if (!start->m_startSettings->m_disableStartPos && start->m_realXPosition > highestPositionX) {
                    selectedStartpos = i;
                    highestPositionX = start->m_realXPosition;
                }
            }

            updateLabel();
        }

        return true;
    }

    void onQuit() {
        startPos.clear();
        selectedStartpos = -1;
        PlayLayer::onQuit();
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



