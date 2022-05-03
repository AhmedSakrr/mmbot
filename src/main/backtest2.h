/*
 * backtest2.h
 *
 *  Created on: 30. 3. 2022
 *      Author: ondra
 */

#ifndef SRC_MAIN_BACKTEST2_H_
#define SRC_MAIN_BACKTEST2_H_

#include <shared/refcnt.h>
#include "trader_factory.h"

#include "spreadgenerator.h"

#include "istrategy3.h"

#include "trader.h"

#include "istockapi.h"


class Backtest: public ondra_shared::RefCntObj {
public:

	Backtest(const Trader_Config_Ex &cfg,
					 const IStockApi::MarketInfo &minfo,
					 double assets,
					 double currency);


	class Source;

	struct Order {
		double price = 0;
		double size = 0;
	};


	void start(std::vector<float> &&prices, std::uint64_t start_time);
	bool next();

	Trader &get_trader();

	Order get_buy_order() const {return buy;}
	Order get_sell_order() const {return sell;}

	double get_cur_price() const {
		return cur_price;
	}


	const IStatSvc::Info& get_info() const {
		return info;
	}

	const IStatSvc::MiscData& get_misc_data() const {
		return miscData;
	}

	double get_position() const {
		return position;
	}


	const ondra_shared::StringView<TradeRecord>& get_trades() const {
		return trades;
	}

	std::uint64_t get_cur_time() const;

	const std::string& getBuyErr() const {
		return buy_err;
	}

	const std::string& getGenErr() const {
		return gen_err;
	}

	const std::string& getSellErr() const {
		return sell_err;
	}

	class Reporting;


	struct LogMsg {
		std::uint64_t time;
		std::string text;
	};


	const std::vector<LogMsg> &get_log_msgs() const;

	double get_sim_balance() const {
		return sim_balance;
	}

	double get_sim_equity() const {
		return sim_equity;
	}

	double get_sim_position() const {
		return sim_position;
	}

	double get_progress() const;





protected:
	Trader_Config_Ex cfg;
	PStockApi source;
	std::unique_ptr<Trader> trader;
	std::vector<float> prices;
	std::size_t pos = 0;
	std::uint64_t start_time = 0;

protected: //reported data;
	ondra_shared::StringView<TradeRecord> trades;
	std::string buy_err, sell_err, gen_err;
	IStatSvc::MiscData miscData;
	IStatSvc::Info info;
	Order buy, sell;
	std::vector<LogMsg> log_msgs;
	double cur_price = 0;
	double position = 0;
	double sim_balance = 0;
	double sim_equity = 0;
	double sim_position = 0;
	bool inverted_trades;





};




#endif /* SRC_MAIN_BACKTEST2_H_ */
