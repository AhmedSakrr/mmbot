/*
 * strategy_stairs.cpp
 *
 *  Created on: 2. 2. 2020
 *      Author: ondra
 */

#include "strategy_stairs.h"

#include <cmath>
#include "../imtjson/src/imtjson/object.h"
#include "sgn.h"
Strategy_Stairs::~Strategy_Stairs() {
	// TODO Auto-generated destructor stub
}

intptr_t Strategy_Stairs::getNextStep(double dir, std::intptr_t prev_dir) const {
	auto cs = st.step;
	auto idir = static_cast<int>(dir);
	if (idir * cs >= 0) {
		cs = cs + idir;
	} else switch (cfg.redmode) {
		case stepsBack:
			if (cfg.reduction>0) cs = cs + idir * cfg.reduction; else cs = 0;break;
		case reverse:
			cs = idir*cfg.reduction; break;
		case lockOnReduce:
			if (cs * idir < -cfg.reduction || prev_dir * dir >= 0) {
				cs = -((cfg.reduction-1) * idir);
			} else {
				cs = idir;
			}
			break;
		case lockOnReverse:
			if (cs * idir < -cfg.reduction)
				cs = idir;
			break;
	}
	return std::min(std::max(cs, -cfg.max_steps), cfg.max_steps);
}

template<typename Fn>
void Strategy_Stairs::serie(Pattern pat, int maxstep, Fn &&fn) {
	int idx;
	double sum;
	switch(pat) {
	case constant: idx = 0;
		while (fn(idx,idx)) idx++;
		break;
	case harmonic: sum = 0; idx=0;
		while (fn(idx,sum)) {++idx; sum = sum + 1.0/idx;}
		break;
	case arithmetic: sum = 0; idx=0;
		while (fn(idx,sum)) {++idx; sum = sum + idx;}
		break;
	case exponencial:
		if (fn(0, 0)) {
			sum = 1;
			idx = 1;
			while (fn(idx,sum)) {sum = sum + sum;++idx;}
		}
		break;
	case parabolic:
		sum = 0;
		idx = 0;
		while (fn(idx, sum)) {
			++idx;
			sum = sum + std::max((maxstep+1-idx)*idx,0);
		}
	}
}

double Strategy_Stairs::stepToPos(std::intptr_t step) const {
	if (cfg.pattern == constant) return step;
	else {
		double mlt = sgn(step);
		std::intptr_t istep = std::abs(step);
		double res = 0;
		serie(cfg.pattern, cfg.max_steps,[&](int idx, double amount){res = amount; return idx < istep;});
		return res*mlt;
	}
}
std::intptr_t Strategy_Stairs::posToStep(double pos) const {
	std::intptr_t s = sgn(pos);
	double p = std::abs(pos);
	std::intptr_t res;
	if (cfg.pattern == constant) {
		res = static_cast<std::intptr_t>(std::round(p)) * s;
	}
	else {
		std::intptr_t r = 0;
		double prevp = 0;
		serie(cfg.pattern,cfg.max_steps, [&](int idx, double amount){
			r = idx-1;
			double diff = amount - prevp;
			double pp = prevp+diff*0.5;
			prevp = amount;
			return pp<=p && r < cfg.max_steps;});
		res = r*s;
	}
	return std::min(std::max(res, -cfg.max_steps), cfg.max_steps);
}



IStrategy::OrderData Strategy_Stairs::getNewOrder(
		const IStockApi::MarketInfo &minfo, double cur_price, double new_price,
		double dir, double assets, double currency) const {

	double power = st.power;
	auto step = st.sl?0:getNextStep(dir, st.prevdir);
	double new_pos = power * stepToPos(step);
	double sz = calcOrderSize(posToAssets(st.pos),assets, posToAssets(	new_pos));
	return OrderData{(st.sl && dir * new_pos < 0)?cur_price:new_price,sz,st.sl?Alert::stoploss:Alert::enabled};
}

bool Strategy_Stairs::isMargin(const IStockApi::MarketInfo& minfo) const {
	switch (cfg.mode) {
	default:
	case autodetect: return minfo.leverage != 0;
	case exchange: return false;
	case margin: return true;
	}

}

std::pair<IStrategy::OnTradeResult, ondra_shared::RefCntPtr<const IStrategy> > Strategy_Stairs::onTrade(
		const IStockApi::MarketInfo &minfo, double tradePrice, double tradeSize,
		double assetsLeft, double currencyLeft) const {

	if (!isValid()) return onIdle(minfo,{tradePrice,tradePrice,tradePrice,0},assetsLeft,currencyLeft)
						->onTrade(minfo,tradePrice,tradeSize,assetsLeft,currencyLeft);
	auto dir = sgn(st.price - tradePrice);
	double power = st.power;
	double curpos = assetsToPos(assetsLeft);
	double prevpos = curpos - tradeSize;
	intptr_t step = curpos == 0?0:getNextStep(dir,st.prevdir);
	State nst = st;
	nst.price = tradePrice;
	nst.pos = stepToPos(step)*power;
	nst.open = prevpos*curpos<=0 ? tradePrice:(st.open*prevpos + tradeSize*tradePrice)/curpos;
	nst.enter = prevpos*curpos<=0 ? tradePrice:curpos * dir > 0?(st.enter*prevpos + tradeSize*tradePrice)/curpos:st.enter;
	nst.step = step;
	nst.prevdir = dir?dir:st.prevdir;
	nst.sl = cfg.sl && step == st.step && dir * step >= cfg.max_steps && tradeSize == 0;
	double spread = std::abs(tradePrice - st.price);
	double posChange = std::abs(tradeSize);
	if (posChange) {
		double value = spread * pow2(nst.pos)/(posChange*2.0);
		double f = posChange / (std::abs(nst.pos)+posChange);
		nst.value = nst.value *  (1-f) + value * f;
	}
	if (nst.pos * st.pos <0 || st.pos == 0) {
		if (isMargin(minfo)) {
			nst.power = calcPower(tradePrice, currencyLeft);
		} else {
			nst.neutral_pos = calcNeutralPos(assetsLeft,currencyLeft, tradePrice, minfo.leverage != 0);
			nst.power = calcPower(tradePrice, nst.neutral_pos * tradePrice);
		}
	}
	return {
		OnTradeResult{
			prevpos * (tradePrice - st.price) + (nst.value - st.value),
			0,nst.enter,nst.open
		},new Strategy_Stairs(cfg,nst)
	};
}

IStrategy::MinMax Strategy_Stairs::calcSafeRange(
		const IStockApi::MarketInfo &minfo, double assets,
		double currencies) const {
	return MinMax {
		0, std::numeric_limits<double>::infinity()
	};
}

bool Strategy_Stairs::isValid() const {
	return st.price > 0 && st.power > 0;
}

json::Value Strategy_Stairs::exportState() const {
	return json::Object("price", st.price)
					   ("pos", st.pos)
					   ("open", st.open)
					   ("enter", st.enter)
					   ("value", st.value)
					   ("neutral_pos", st.neutral_pos)
					   ("power", st.power)
					   ("step", st.step)
					   ("prev_dir", st.prevdir)
					   ("cfg_hash", st.cfghash)
					   ("sl",st.sl)
					   ;
}

double Strategy_Stairs::calcPower(double price, double currency) const {
	double steps = stepToPos(cfg.max_steps);
	return (currency / price * std::pow(10, cfg.power)*0.01)/steps;
}

PStrategy Strategy_Stairs::onIdle(const IStockApi::MarketInfo &minfo,
		const IStockApi::Ticker &curTicker, double assets,
		double currency) const {
	if (isValid()) return this;
	else {
		PStrategy g;
		State nst;
		if (isMargin(minfo)) {
			double ps = calcPower(curTicker.last, currency);
			nst = State{curTicker.last, assets, curTicker.last, curTicker.last, 0,0, ps,  0};
		} else {
			double neutral_pos = calcNeutralPos(assets,currency, curTicker.last, minfo.leverage != 0);
			double ps = calcPower(curTicker.last, neutral_pos * curTicker.last);
			double pos = assets-neutral_pos;
			nst =  State {curTicker.last, pos, curTicker.last, curTicker.last, 0,neutral_pos, ps,  0};
		}
		nst.step = posToStep(assets/nst.power);
		nst.cfghash = getCfgHash();
		g = new Strategy_Stairs(cfg, nst);
		if (g->isValid()) return g;
		else throw std::runtime_error("Stairs: Invalid settings - unable to initialize strategy");
	}

}

double Strategy_Stairs::getEquilibrium() const {
	return st.price;
}

PStrategy Strategy_Stairs::reset() const {
	return new Strategy_Stairs(cfg);
}

Strategy_Stairs::Strategy_Stairs(const Config &cfg):cfg(cfg) {
}

Strategy_Stairs::Strategy_Stairs(const Config &cfg, const State &state):cfg(cfg),st(state) {
}

std::string_view Strategy_Stairs::id = "stairs";

std::string_view  Strategy_Stairs::getID() const {
	return id;
}

json::Value Strategy_Stairs::dumpStatePretty(
		const IStockApi::MarketInfo &minfo) const {

	return json::Object
			("Last price", minfo.invert_price?1.0/st.price:st.price)
			("Position", minfo.invert_price?-st.pos:st.pos)
			("Step", minfo.invert_price?-st.step:st.step)
			("Step width", st.power)
			("Neutral position", minfo.invert_price?-st.neutral_pos:st.neutral_pos)
			("Open price", minfo.invert_price?1.0/st.open:st.open);

}

PStrategy Strategy_Stairs::importState(json::Value src) const {
	State newst{
		src["price"].getNumber(),
		src["pos"].getNumber(),
		src["open"].getNumber(),
		src["enter"].getNumber(),
		src["value"].getNumber(),
		src["neutral_pos"].getNumber(),
		src["power"].getNumber(),
		src["step"].getInt(),
		src["prev_dir"].getInt(),
		src["cfg_hash"].getUInt(),
		src["sl"].getBool()
	};
	if (newst.cfghash != getCfgHash()) {
		newst.price = 0;
		newst.power = 0;
	}

	return new Strategy_Stairs(cfg, newst);
}
double Strategy_Stairs::assetsToPos(double assets) const {
	return assets - st.neutral_pos;
}
double Strategy_Stairs::posToAssets(double pos) const {
	return pos + st.neutral_pos;
}

double Strategy_Stairs::calcNeutralPos(double assets, double currency, double price, bool leverage) const {
	double value = leverage?currency:(assets * price + currency);
	double middle = value/2;
	return middle/price;
}

std::size_t Strategy_Stairs::getCfgHash() const {
	json::Value data = {
			cfg.power,
			(int)cfg.pattern,
			cfg.max_steps,
			cfg.reduction,
			(int)cfg.mode,
			(int)cfg.redmode
	};
	return std::hash<json::Value>()(data);
}
