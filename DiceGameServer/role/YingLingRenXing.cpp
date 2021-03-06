#include "YingLingRenXing.h"
#include "..\GameGrail.h"
#include "..\UserTask.h"
#include <set>

enum {
	ELEMENT_ALL_DIFFERENT = 1,
	ELEMENT_ALL_THE_SAME = 2,
	ELEMENT_INVALID = 3,
};
YingLingRenXing::YingLingRenXing(GameGrail *engine, int id, int color): PlayerEntity(engine, id, color)
	{
		tokenMax[0]=3;
		tokenMax[1]=3;
		token[0]=3;
		GameInfo game_info;
		Coder::tokenNotice(id, 0, token[0], game_info);
		engine->sendMessage(-1, MSG_GAME, game_info);
	};
bool YingLingRenXing::cmdMsgParse(UserTask* session, uint16_t type, ::google::protobuf::Message *proto)
{
	switch(type)
	{
	case MSG_RESPOND:
		Respond* respond = (Respond*)proto;
		switch(respond->respond_id())
		{
		case NU_HUO_MO_WEN :
			session->tryNotify(id, STATE_TIMELINE_2_MISS,NU_HUO_MO_WEN, respond);
			return true;
			break;
		case ZHAN_WEN_SUI_JI :
			session->tryNotify(id,STATE_TIMELINE_2_HIT,ZHAN_WEN_SUI_JI, respond);
			return true;
			break;
		case SHUANG_CHONG_HUI_XIANG:
			session->tryNotify(id,STATE_TIMELINE_3,SHUANG_CHONG_HUI_XIANG, respond);
			return true;
			break;
		case FU_WEN_GAI_ZAO:
			session->tryNotify(id,STATE_BOOT,FU_WEN_GAI_ZAO, respond);
			return true;
			break;
		case FU_WEN_GAI_ZAO_TOKEN:
			session->tryNotify(id,STATE_BOOT,FU_WEN_GAI_ZAO_TOKEN,respond);
			return true;
			break;
		}
	}
	//没匹配则返回false
	return false;
}

int YingLingRenXing::p_timeline_2_hit(int &step, CONTEXT_TIMELINE_2_HIT *con)
{
	if(con->attack.srcID != id || !con->attack.isActive || token[0] == 0) {
		return GE_SUCCESS;
	}
	step = ZHAN_WEN_SUI_JI;
	int ret = ZhanWenSuiJi(con);
	if(toNextStep(ret)||ret == GE_URGENT) {
		step = STEP_DONE;
	}
	return ret; 
}

int YingLingRenXing::p_timeline_2_miss(int &step, CONTEXT_TIMELINE_2_MISS *con)
{
	if(con->srcID != id || !con->isActive) {
		return GE_SUCCESS;
	}
	step = NU_HUO_MO_WEN;
	int ret = NuHuoMoWen(con);
	if(toNextStep(ret)||ret == GE_URGENT) {
		step = STEP_DONE;
	}
	return ret;
}

int YingLingRenXing::p_timeline_3(int &step, CONTEXT_TIMELINE_3 *con)
{
	if(con->harm.srcID != id || getEnergy()<1) {
		return GE_SUCCESS;
	}
	step = SHUANG_CHONG_HUI_XIANG;
	int ret = ShuangChongHuiXiang(con);
	if(toNextStep(ret)||ret == GE_URGENT) {
		step = STEP_DONE;
	}
	return ret;
}

int YingLingRenXing::p_turn_begin(int &step, int currentPlayerID)
{
	shuangChongHuiXiangUsed = false;
	return GE_SUCCESS;
}

int YingLingRenXing::p_boot(int &step, int currentPlayerID)
{
	if(currentPlayerID != id) {
		return GE_SUCCESS;
	}
	int ret = GE_INVALID_STEP;
	if(step == STEP_INIT)
	{
		step = FU_WEN_GAI_ZAO;
	}
	if(step == FU_WEN_GAI_ZAO)
	{
		ret = FuWenGaiZao();
		if(toNextStep(ret)) {
			step = STEP_DONE;
		}
		else if(ret == GE_URGENT)
		{
			step = FU_WEN_GAI_ZAO_TOKEN;
		}
		if(ret != GE_SUCCESS)
			return ret;
	}
	if(step == FU_WEN_GAI_ZAO_TOKEN)
	{
		ret = FuWenGaiZaoToken();
		if(toNextStep(ret)) {
			step = STEP_DONE;
		}
	}
	return ret;
}

int YingLingRenXing::p_turn_end(int &step, int currentPlayerID)
{
	if(currentPlayerID != id || !tap) {
		return GE_SUCCESS;
	}
	step = FU_WEN_GAI_ZAO;
	int ret = FuWenTurnEnd();
	if(toNextStep(ret) || ret == GE_URGENT) {
		step = STEP_DONE;
	}
	return ret;
}

int YingLingRenXing::p_before_lose_morale(int &step, CONTEXT_LOSE_MORALE *con)
{
	if(con->harm.cause != SHUANG_CHONG_HUI_XIANG)
		return GE_SUCCESS;
	step = SHUANG_CHONG_HUI_XIANG;
	int ret = ShuangChongHuiXiangMorale(con);
	if(toNextStep(ret)) {
		step = STEP_DONE;
	}
	return ret;
}

int YingLingRenXing::NuHuoMoWen(CONTEXT_TIMELINE_2_MISS *con)
{
	int ret;
	int dstID = con->dstID;
	CommandRequest cmd_req;
	Coder::askForSkill(id, NU_HUO_MO_WEN, cmd_req);
	if(engine->waitForOne(id, network::MSG_CMD_REQ, cmd_req))
	{
		void* reply;
		if (GE_SUCCESS == (ret = engine->getReply(id, reply)))
		{
			Respond* respond = (Respond*) reply;
			//怒火压制
			if(respond->args(0)==1)
			{
				if(token[0] < 1) {
					return GE_INVALID_ARGUMENT;
				}
				SkillMsg skill;
				Coder::skillNotice(id, dstID, NU_HUO_YA_ZHI, skill);
				engine->sendMessage(-1, MSG_SKILL, skill);
				setToken(0, token[0]-1);
				setToken(1, token[1]+1);
				GameInfo game_info;
				Coder::tokenNotice(id, 0, token[0], game_info);
				Coder::tokenNotice(id, 1, token[1], game_info);
				engine->sendMessage(-1, MSG_GAME, game_info);
				return GE_SUCCESS;
			}
			//魔纹融合
			if(respond->args(0)==2)
			{
				int cardNum = respond->card_ids_size();
				vector<int> cardIDs;
				if(token[1] < 1 || cardNum < 2) {
					return GE_INVALID_ARGUMENT;
				}
				for(int i = 0; i < cardNum;i ++)
				{
					cardIDs.push_back(respond->card_ids(i));
				}
				if(elementCheck(cardIDs) != ELEMENT_ALL_DIFFERENT || GE_SUCCESS != checkHandCards(cardIDs.size(), cardIDs)) {
					return GE_INVALID_CARDID;
				}
				setToken(0, token[0]+1);
				setToken(1, token[1]-1);

				int additionMoWen = respond->args(1);
				if(additionMoWen > 0) {
					if(token[1] < additionMoWen) {
						return GE_INVALID_ARGUMENT;
					}
					setToken(0, token[0]+additionMoWen);
					setToken(1, token[1]-additionMoWen);
				}

				HARM moWenHarm;
				moWenHarm.type = HARM_MAGIC;
				moWenHarm.point = cardNum - 1 + additionMoWen;
				moWenHarm.srcID = id;
				moWenHarm.cause = MO_WEN_RONG_HE;

				SkillMsg skill;
				Coder::skillNotice(id, dstID, MO_WEN_RONG_HE, skill);
				engine->sendMessage(-1, MSG_SKILL, skill);
				GameInfo game_info;
				Coder::tokenNotice(id, 0, token[0], game_info);
				Coder::tokenNotice(id, 1, token[1], game_info);
				engine->sendMessage(-1, MSG_GAME, game_info);
				CardMsg show_card;
				Coder::showCardNotice(id, cardNum, cardIDs, show_card);
				engine->sendMessage(-1, MSG_CARD, show_card);

				engine->setStateTimeline3(dstID, moWenHarm);
				engine->setStateMoveCardsNotToHand(id, DECK_HAND, -1, DECK_DISCARD, cardNum, cardIDs, id, MO_WEN_RONG_HE, true);
				return GE_URGENT;
			}
			//不发动
			return GE_SUCCESS;
		}
		return ret;
	}
	else {
		return GE_TIMEOUT;
	}
}

int YingLingRenXing::ZhanWenSuiJi(CONTEXT_TIMELINE_2_HIT *con)
{
	int ret;
	int dstID = con->attack.dstID;
	CommandRequest cmd_req;
	Coder::askForSkill(id, ZHAN_WEN_SUI_JI, cmd_req);
	if(engine->waitForOne(id, network::MSG_CMD_REQ, cmd_req))
	{
		void* reply;
		if (GE_SUCCESS == (ret = engine->getReply(id, reply)))
		{
			Respond* respond = (Respond*) reply;
			//战纹碎击
			if(respond->args(0)==1)
			{
				int cardNum = respond->card_ids_size();
				vector<int> cardIDs;
				if(token[0] < 1 || cardNum < 2) {
					return GE_INVALID_ARGUMENT;
				}
				for(int i = 0; i < cardNum;i ++)
				{
					cardIDs.push_back(respond->card_ids(i));
				}
				if(elementCheck(cardIDs) != ELEMENT_ALL_THE_SAME || GE_SUCCESS != checkHandCards(cardIDs.size(), cardIDs)) {
					return GE_INVALID_CARDID;
				}
				setToken(0, token[0]-1);
				setToken(1, token[1]+1);

				int additionZhanWen = respond->args(1);
				if(additionZhanWen > 0) {
					if(token[0] < additionZhanWen) {
						return GE_INVALID_ARGUMENT;
					}
					setToken(0, token[0]-additionZhanWen);
					setToken(1, token[1]+additionZhanWen);
				}


				SkillMsg skill;
				Coder::skillNotice(id, dstID, ZHAN_WEN_SUI_JI, skill);
				engine->sendMessage(-1, MSG_SKILL, skill);
				GameInfo game_info;
				Coder::tokenNotice(id, 0, token[0], game_info);
				Coder::tokenNotice(id, 1, token[1], game_info);
				engine->sendMessage(-1, MSG_GAME, game_info);
				CardMsg show_card;
				Coder::showCardNotice(id, cardNum, cardIDs, show_card);
				engine->sendMessage(-1, MSG_CARD, show_card);
				engine->setStateMoveCardsNotToHand(id, DECK_HAND, -1, DECK_DISCARD, cardNum, cardIDs, id, ZHAN_WEN_SUI_JI, true);
				con->harm.point += cardNum - 1 + additionZhanWen;
				return GE_URGENT;
			}
			//不发动
			return GE_SUCCESS;
		}
		return ret;
	}
	else {
		return GE_TIMEOUT;
	}
}

int YingLingRenXing::FuWenGaiZao()
{
	if(gem < 1)
		return GE_SUCCESS;
	int ret;
	CommandRequest cmd_req;
	Coder::askForSkill(id, FU_WEN_GAI_ZAO, cmd_req);
	if(engine->waitForOne(id, network::MSG_CMD_REQ, cmd_req))
	{
		void* reply;
		if (GE_SUCCESS == (ret = engine->getReply(id, reply)))
		{
			Respond* respond = (Respond*) reply;
			if(respond->args(0) == 1)
			{
				SkillMsg skill;
				Coder::skillNotice(id, id, FU_WEN_GAI_ZAO, skill);
				engine->sendMessage(-1, MSG_SKILL, skill);
				setGem(gem-1);
				tap = true;

				GameInfo game_info;
				Coder::energyNotice(id, gem, crystal, game_info);
				Coder::tapNotice(id, tap, game_info);
				engine->sendMessage(-1, MSG_GAME, game_info);

				vector<int> cards;
				HARM harm;
				harm.srcID = id;
				harm.type = HARM_NONE;
				harm.point = 2;
				harm.cause = FU_WEN_GAI_ZAO;
				engine->setStateMoveCardsToHand(-1, DECK_PILE, id, DECK_HAND, 2, cards, harm, false);
				engine->setStateChangeMaxHand(id, false, false, 6, 2);
				return GE_URGENT;
			}
			else {
				return GE_SUCCESS;
			}
		}
		return ret;
	}
	else {
		return GE_TIMEOUT;
	}
}

int YingLingRenXing::FuWenGaiZaoToken()
{
	int ret;
	CommandRequest cmd_req;
	Coder::askForSkill(id, FU_WEN_GAI_ZAO_TOKEN, cmd_req);
	if(engine->waitForOne(id, network::MSG_CMD_REQ, cmd_req))
	{
		void* reply;
		if (GE_SUCCESS == (ret = engine->getReply(id, reply)))
		{
			Respond* respond = (Respond*) reply;
			int num = respond->args(1);
			if(num < 0 || num > 3)
				return GE_INVALID_ARGUMENT;
			setToken(0, num);
			setToken(1, 3-token[0]);
			GameInfo game_info;
			Coder::tokenNotice(id, 0, token[0], game_info);
			Coder::tokenNotice(id, 1, token[1], game_info);
			engine->sendMessage(-1, MSG_GAME, game_info);
			return GE_SUCCESS;
		}
		return ret;
	}
	else
	{
		int num = 1;
		setToken(0, num);
		setToken(1, 3-token[0]);
		GameInfo game_info;
		Coder::tokenNotice(id, 0, token[0], game_info);
		Coder::tokenNotice(id, 1, token[1], game_info);
		engine->sendMessage(-1, MSG_GAME, game_info);
		return GE_TIMEOUT;
	}
}

int YingLingRenXing::FuWenTurnEnd()
{
	if(tap)
	{
		SkillMsg skill;
		Coder::skillNotice(id, id, FU_WEN_GAI_ZAO, skill);
		engine->sendMessage(-1, MSG_SKILL, skill);
		tap = false;
		GameInfo game_info;
		Coder::tapNotice(id, tap, game_info);
		engine->sendMessage(-1, MSG_GAME, game_info);
		engine->setStateChangeMaxHand(id, false, false, 6, -2);
		return GE_URGENT;
	}
	else {
		return GE_SUCCESS;
	}
}

int YingLingRenXing::ShuangChongHuiXiang(CONTEXT_TIMELINE_3 *con)
{
	if(con->harm.srcID != id || getEnergy() < 1 || shuangChongHuiXiangUsed){
		return GE_SUCCESS;
	}
	int ret;
	CommandRequest cmd_req;
	Coder::askForSkill(id,SHUANG_CHONG_HUI_XIANG, cmd_req);
	Command *cmd = (Command*)(&cmd_req.commands(cmd_req.commands_size()-1));
	cmd->add_args(con->dstID);
	if(engine->waitForOne(id, network::MSG_CMD_REQ, cmd_req))
	{
		void* reply;
		if (GE_SUCCESS == (ret = engine->getReply(id, reply)))
		{
			Respond* respond = (Respond*) reply;
			if(respond->args(0) == 1)
			{
				int dstID = respond->dst_ids(0);
				if(dstID == con->dstID)
					return GE_INVALID_ARGUMENT;
				SkillMsg skill;
				Coder::skillNotice(id, dstID, SHUANG_CHONG_HUI_XIANG, skill);
				engine->sendMessage(-1, MSG_SKILL, skill);
				shuangChongHuiXiangUsed = true;
				if(getCrystal()>0) {
					setCrystal(crystal-1);
				}
				else {
					setGem(gem-1);
				}
				GameInfo game_info;
				Coder::energyNotice(id, gem, crystal, game_info);
				engine->sendMessage(-1, MSG_GAME, game_info);

				HARM harm;
				harm.srcID = id;
				harm.type = HARM_MAGIC;
				harm.point = con->harm.point<3? con->harm.point:3;
				harm.cause = SHUANG_CHONG_HUI_XIANG;
				engine->setStateTimeline3(dstID, harm);
				return GE_URGENT;
			}
			return GE_SUCCESS;
		}
		return ret;
	}
	else {
		return GE_TIMEOUT;
	}
}

int YingLingRenXing::ShuangChongHuiXiangMorale(CONTEXT_LOSE_MORALE *con)
{
	if(con->harm.cause != SHUANG_CHONG_HUI_XIANG) {
		return GE_SUCCESS;
	}
	con->howMany = 0;
	return GE_SUCCESS;
}

int YingLingRenXing::elementCheck(vector<int> cards)
{
	vector<int>::iterator card_it;
	CardEntity* card;
	set<int> elements;
	for (card_it = cards.begin(); card_it != cards.end(); ++card_it)
	{
		card = getCardByID(*card_it);
		elements.insert(card->getElement());
	}
	if (elements.size() == 1)
		return ELEMENT_ALL_THE_SAME;
	if (elements.size() == cards.size())
		return ELEMENT_ALL_DIFFERENT;
	return ELEMENT_INVALID;
}