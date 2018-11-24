
# -*- coding: utf-8 -*-

# 函数说明
#
# 函数：result = LoadQuote(file_path, file_head, file_code, data_code, quote_list, list_info, dict_data)
#
# 功能: 从落地转储文件导入LTB实时个股快照行情数据。
#
# 入参：file_path  <- 落地转储文件路径。
#　 　　file_head  <- 落地转储文件头部字节长度。
#　 　　file_code  <- 落地转储文件数据压缩格式，未压缩则置空字符串。
#　 　　data_code  <- 落地转储行情数据字符编码，utf8 或 gbk 等。
#　 　　quote_list <- 需要筛选的合约列表，英文逗号分隔，为空表示全市场，注意字母大小写。
#
# 返回：result     -> 函数调用执行结果，失败返回：-1，成功返回：>= 0。
#　 　　list_info  -> 函数调用执行信息。
#　 　　dict_data  -> LTB实时个股快照行情数据。
#
# 数据："Code", "Name", "Type", "Market", "Status", 
#　 　　"Last", "Open", "High", "Low", "Close", "PreClose", "Volume", "Turnover", 
#　 　　"AskPrice[10]", "AskVolume[10]", "BidPrice[10]", "BidVolume[10]", 
#　 　　"HighLimit", "LowLimit", "OpenInterest", "IOPV", "TradeCount", "YieldToMaturity", "AuctionPrice", 
#　 　　"BidPriceLevel", "OfferPriceLevel", "TotalBidVolume", "TotalOfferVolume",
#　 　　"WeightedAvgBidPrice", "WeightedAvgOfferPrice", "AltWeightedAvgBidPrice", "AltWeightedAvgOfferPrice", "TradingPhase", "OpenRestriction", 
#　 　　"QuoteTime", "LocalTime", "LocalIndex"。
#　 　　具体行情数据格式请参考相关业务文档。

import LoadQuoteStockSnapLTB

file_head = 32
file_code = "lzma"
data_code = "utf8"
file_path = "C:\\Users\\xrd\\Desktop\\20160622.hq.lzma"

list_info = []
dict_data = {}

#result = LoadQuoteStockSnapLTB.LoadQuote(file_path, file_head, file_code, data_code, "", list_info, dict_data) # 注意内存哦
result = LoadQuoteStockSnapLTB.LoadQuote(file_path, file_head, file_code, data_code, "600000, 000001, 600004, 000005", list_info, dict_data)
print(result, len(list_info))
if result >= 0:
    for i in range(result):
        print(dict_data["Code"][i], dict_data["Name"][i], dict_data["Type"][i], dict_data["Market"][i], dict_data["Status"][i], \
              dict_data["Last"][i], dict_data["Close"][i], dict_data["Volume"][i], dict_data["Turnover"][i])
        for j in range(10):
            print(dict_data["AskPrice"][j][i], dict_data["AskVolume"][j][i], dict_data["BidPrice"][j][i], dict_data["BidVolume"][j][i])
        print(dict_data["QuoteTime"][i], dict_data["LocalTime"][i], dict_data["LocalIndex"][i])
    for i in range(len(list_info)):
        print("成功：", list_info[i])
else:
    for i in range(len(list_info)):
        print("失败：", list_info[i])
