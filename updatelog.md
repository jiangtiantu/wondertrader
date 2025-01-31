### 0.3.4
* 正式开源的第一个版本


### 0.3.5
* 把手数相关的都从整数改成浮点数，主要目的是为了以后兼容虚拟币交易(虚拟币交易数量都是小数单位)
* 优化手数改成浮点数以后带来的日志输出不简洁的问题(浮点数打印会显示很多“0000”)
* 逐步完善文档
* XTP实盘适配，主要是修复bug

### 0.3.6
* 执行器使用线程池，减少对网络线程的时间占用
* 增加了一个实盘仿真模块TraderMocker，可以满足目前已经支持的股票和期货的仿真交易
* 更多接口支持（飞马、tap、CTPMini）
* 内置执行算法增加TWAP
* 继续完善文档

### 0.4.0
* 新增一个**选股调度引擎**，用于调度应用层的选股策略，得到目标组合以后，提供自动执行服务，暂时只支持日级别以上的调度周期，执行会放到第二天
* 因为新增了选股调度引擎，所以全面重构`WtPorter`和`WtBtPorter`导出的接口函数，以便调用的时候区分
* 新增一个**独立的执行器模块**`WtExecMon`，并导出C接口提供服务。主要是剥离了策略引擎逻辑，提供单纯的执行服务，方便作为单纯的执行通道，嫁接到其他框架之下
* `Windows`下的开发环境从`vs2013`升级到`vs2017`，`boost1.72`和`curl`需要同步升级


### 0.5.0
* 股票数据新增前复权因子处理逻辑，直接从未复权数据转换成前复权数据
* `demos`目录下，将`python`的demo迁移到`wtpy`之下，然后新增一个`C++`版本demo的解决方案，提供了cta策略、高频策略、执行单元、选股策略四个demo，并提供了一个回测入口程序，用于本地调试
* 日志模块调整,主要是利用`fmtlib`优化了一些日志输出的细节
* 完成了高频策略引擎C接口导出

### 0.5.1
* 实盘引擎（`CTA`、`HFT`、`SEL`）在启动的时候，将策略列表和交易通道列表输出到一个配置文件中，方便监控服务读取查看
* 新增一个事件通知组件`EventNotifier`，主要作用是通过`UDP`通道，向指定的接受端发送成交回报、订单回报，以后还会扩展到其他盘中需要监控的数据
* 回测引擎，回测过程中产生的数据记录（成交、信号、平仓、资金）不在回测过程中写入文件，而是到了回测结束以后统一写入文件
* 完善了系统中合约代码标准化对股指期权IO的处理

### 0.5.2
* 修改`CTPLoader`为显式加载`CTP`模块，方便设置`CTP`模块的路径
* 将所有的通道对接模块（行情、交易）改成显示加载三方依赖库，并统一检查了版本的一致性
* 修正了股票`Level2`数据落地的一些细节问题
* 修正了风控触发时，限制手数比例为小数时，没有进行四舍五入导致的问题
* 历史数据新增了对`Mysql`数据库的支持，涉及到的模块包括数据读取模块`WtDataReader`、数据落地模块`WtDataWriter`、回测框架`WtBtCore`，`Mysql`库版本为`6.11`
* 修正了`UDP`广播模块中，中转的广播消息对象对`WTSObject`的处理的`bug`

### 0.5.3
* 回测引擎增加了设置成交滑点的参数选项，不设置则为0
* 修正了`C++demo`中的一些代码的细节问题
* 执行模块为搭建分布式执行框架做了一些预先调整
* `ParserUDP`模块接收缓存改成`8M`
* 增加了一个`MiniLoader`工程，用于从`CTPMini`接口拉取合约列表
* 将`linux`下编译的`boost`依赖从动态库改成静态库
* 其他细节完善

### 0.5.4
* `WtBtPorter`、`WtPorter`、`WtExecMon`的初始化接口，全部改成支持传文件名和文件内容两种方式
* `CTA`实盘引擎中，策略发出信号的时候，新增了一个订阅`tick`的操作，主要针对策略`交易未订阅K线的品种`的需求
* 优化了`Windows`下`dmp`文件生成的路径，方便调试`bug`
* 回测引擎中，成交明细和平仓明细，新增了一个`BarNumber`的字段，主要用于统计每个交易回合的周期数，`BarNumber`指的是主K线的`BarNumber`，并且是一个相对开始回测的第一条K线的编号。
* 回测引擎中，针对`CTA`策略`交易未订阅K线的品种`的需求做了一些优化
* 全平台中，将能部分`boost`库改成`std`的库，减少对`boost`的依赖
* 新增一个`WtDtHelper`模块，主要提供数据辅助功能，目前主要是提供`csv`和二进制文件的互转，后面还会加入数据库、二进制、`csv`的互转接口
* 将平台版本号从`WTSMarcos.h`迁移到`WTSVersion.h`中，减少修改版本号引起的重编译

### 0.6.0(大版本)
* `CTA`策略接口新增一个获取交易日的`API`，`cta_get_tdate`和`sel_get_tdate`
* 回测引擎和实盘引擎的C接口，新增`cta_get_all_position`和`sel_get_all_position`两个策略接口，用于获取策略全部持仓
* 修复了`ParserUDP`的一些`bug`
* `CTA`引擎设置目标仓位时，同时订阅`tick`数据，主要针对标的不确定的策略，例如截面因子`CTA`策略
* 内置执行单元`WtSimpExeUnit`新增一个根据`microprice`来确定委托价格的方式
* 将内置执行模块`WtExeFact`中的订单管理模块独立出来，方便调用
* `CTA`回测引擎中，输出的平仓明细中新增“最大潜在收益”和“最大潜在亏损”两个字段
* 回测框架中，将`ExecMocker`中的模拟撮合逻辑剥离出来，放到一个单独的`MatchEngine`中，方便以后的优化
* **`HFT`引擎的回测进行了一次彻底的整理实现，基本满足了`HFT`策略回测的需求（已测试）**
* 初步完成了`HFT`引擎对股票`Level2`数据（`orderqueue`,`orderdetail`,`transaction`）的访问接口（尚未充分测试）
* `WtPorter`和`WtBtPorter`两个C接口粘合模块，初步完成了C接口对股票`Level2`数据的支持
* 其他代码细节的完善


### 0.6.1
* 新增一个/dist目录，用于发布一些程序的执行环境的配置文件
* 将CTA、HFT和SEL引擎的策略新增on_session_begin和on_session_end用于向策略推送交易日开始和交易日结束的事件
* 完善了CTPLoader和MiniLoader，主要优化了对期权合约的支持
* 新增一个CTPOptLoader工程，主要用于CTP股票期权API接入
* 添加了CTP期权接口的行情接入模块ParserCTPOpt以及交易模块TraderCTPOpt
* 初步扩充了交易接口中的期权业务接口，同时修改了一些接口函数命名规则
* 完善了平台中对ETF期权和个股期权的支持，主要修改的点是，ETF期权和个股期权只支持标准代码格式，即SSE.ETFO.10003045，而简写格式如SSE.600000只针对股票
* WtDtHelper增加两个接口,read_dsb_ticks用于读取dsb格式的历史tick数据，read_dsb_bars用于读取dsb格式的历史K线数据
* 创建HFT策略的时候增加一个是否托管数据的参数agent，用于控制是否将持仓、成交等数据放在底层进行管理，默认是托管
* CTA引擎新增一个获取最后一次出场时间的接口stra_get_last_exittime
* 同步更新/demos下的代码
* 其他代码细节的完善

### 0.6.2
* 将日志全部翻译成英文
* 内置简单执行单元WtSimpExeUnit增加了涨跌停价的修正逻辑
* 内置执行单元工厂WtExeFact中的订单管理模块WtOrdMon，检查订单超时时，会根据是否是涨跌停价挂单，如果是涨跌停价挂单，则不进行撤单重挂
* CTPMini2对接模块ParserCTPMini和TraderCTPMini进行的细节完善，并接入实盘
* 文档做了一次更新
* 其他代码细节完善