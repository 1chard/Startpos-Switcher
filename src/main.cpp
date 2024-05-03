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

CCLabelBMFont* label;
CCMenu* menu;

$execute
{
    #ifdef GEODE_IS_ANDROID64
    //1F2003D5
    Mod::get()->patch(reinterpret_cast<void *>(geode::base::get() + 0x82803c), {0x1F, 0x20, 0x03, 0xD5});
    #endif
}

void updateLabel(){
    std::stringstream ss;
    ss << selectedStartpos + 1;
    ss << "/";
    ss << startPos.size();

    label->setString(ss.str().c_str());
}

void switchToStartpos(int incBy)
{
    if(!levelStarted)
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

class StartposSwitcher
{
    public:
        void onLeft(CCObject*)
        {
            switchToStartpos(-1);
            
        }

        void onRight(CCObject*)
        {
            switchToStartpos(1);
        }
};

void onDown(enumKeyCodes key)
{
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

    #ifndef GEODE_IS_ANDROID

    if (!PlayLayer::get()->m_player1->m_isPlatformer)
    {
        //leftKeys.push_back(enumKeyCodes::KEY_W);
        leftKeys.push_back(enumKeyCodes::KEY_Left);

        //rightKeys.push_back(enumKeyCodes::KEY_D);
        rightKeys.push_back(enumKeyCodes::KEY_Right);
    }

    if (!PlayLayer::get()->m_isPracticeMode)
    {
        leftKeys.push_back(enumKeyCodes::KEY_Z);

        rightKeys.push_back(enumKeyCodes::KEY_X);
    }

    #endif

    bool left = false;
    bool right = false;

    for (size_t i = 0; i < leftKeys.size(); i++)
    {
        if (key == leftKeys[i])
            left = true;
    }

    for (size_t i = 0; i < rightKeys.size(); i++)
    {
        if (key == rightKeys[i])
            right = true;
    }
    

    if (left)
    {
        switchToStartpos(-1);
    }

    if (right)
    {
        switchToStartpos(1);
    }
}

class $modify (CCKeyboardDispatcher)
{
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool idk)
    {
        if (!CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, idk))     
            return false;

        if (PlayLayer::get() && down && !idk)
        {
            onDown(key);
        }

        return true;
    }
};

class $modify(PlayLayer)
{
    static PlayLayer* create(GJGameLevel* p0, bool p1, bool p2)
    {
        startPos.clear();
        selectedStartpos = -1;
        levelStarted = false;
        shouldDefaultStartpos = true;

        auto res = PlayLayer::create(p0, p1, p2);

        if (startPos.size() == 0)
            menu->setVisible(false);
        else {
            if(Mod::get()->getSettingValue<bool>("sort-triggers")){
                std::sort(startPos.begin(), startPos.end(), [](StartPosObject* a1, StartPosObject* a2){
                    return a1->getPositionX() < a2->getPositionX();
                });
            }
        
            float highestPositionX = 0.f;

            for (size_t i = 0; i < startPos.size(); i++)
            {
                auto start = startPos[i];

                if(!start->m_startSettings->m_disableStartPos && start->getPositionX() > highestPositionX){
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

    void delayedResetLevel(){
        if(shouldDefaultStartpos){
            // we put current startpos here instead of init() so it does not loop
            StartPosObject* startPosObject = selectedStartpos == -1 ? nullptr : startPos[selectedStartpos];
            PlayLayer::setStartPosObject(startPosObject);
        }

        PlayLayer::delayedResetLevel();
    }

    void startGame(){ // this function trigger when player start to move at attempt 1
        if(!levelStarted) // unlock switcher
            levelStarted = true;

        PlayLayer::startGame();
    }

    void resetLevel()
    {
        PlayLayer::resetLevel();
        PlayLayer::prepareMusic(false);
    } 
};

class $modify (UILayer)
{
    bool init(GJBaseGameLayer* idk)
    {
        if (!UILayer::init(idk))
            return false;

        if (!typeinfo_cast<PlayLayer*>(idk))
            return true;

        menu = CCMenu::create();
        menu->setPosition(ccp(CCDirector::get()->getWinSize().width / 2, 25));
        menu->setContentSize(ccp(0, 0));
        menu->setScale(0.6f);
        menu->setVisible(Mod::get()->getSettingValue<bool>("show-ui"));

        label = CCLabelBMFont::create("0/0", "bigFont.fnt");
        label->setPosition(ccp(0, 0));
        label->setOpacity(100);

        auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
        leftSpr->setOpacity(100);

        auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, menu, menu_selector(StartposSwitcher::onLeft));
        leftBtn->setContentSize(leftBtn->getContentSize() * 5);
        leftSpr->setPosition(leftBtn->getContentSize() / 2);
        leftBtn->setPosition(ccp(-85, 0));

        auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
        rightSpr->setFlipX(true);
        rightSpr->setOpacity(100);

        auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, menu, menu_selector(StartposSwitcher::onRight));
        rightBtn->setContentSize(rightBtn->getContentSize() * 5);
        rightSpr->setPosition(rightBtn->getContentSize() / 2);
        rightBtn->setPosition(ccp(85, 0));

        if (PlatformToolbox::isControllerConnected())
        {
            auto leftCtrl = CCSprite::createWithSpriteFrameName("controllerBtn_DPad_Left_001.png");
            leftCtrl->setPosition(ccp(0, -15));
            leftSpr->addChild(leftCtrl);

            auto rightCtrl = CCSprite::createWithSpriteFrameName("controllerBtn_DPad_Right_001.png");
            rightCtrl->setPosition(ccp(0, -15));
            rightSpr->addChild(rightCtrl);
        }

        menu->addChild(label);
        menu->addChild(leftBtn);
        menu->addChild(rightBtn);
        this->addChild(menu);
        return true;
    }
};

class $modify (StartPosObject)
{
    virtual bool init()
    {
        if (!StartPosObject::init())
            return false;

        if (auto plr = PlayLayer::get())
        {
            startPos.push_back(static_cast<StartPosObject*>(this));
        }

        return true;
    }
};