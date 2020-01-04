struct tm {
	int tm_sec;		/* 秒 [0, 59] */
	int tm_min;		/* 分 [0, 59] */
	int tm_hour;	/* 小时 [0, 59]？ */
	int tm_mday;	/* 一个月的天数 [0, 31] */
	int tm_mon;		/* 一年中的月份 [0, 11] */
	int tm_year;	/* 从1900年开始的年数 */
	int tm_wday;	/* 一个星期中的某一天 [0, 6]（星期天 = 0） */
	int tm_yday;	/* 一年中的某一天 [0, 365] */
	int tm_isdst;	/* 夏令时标志？正数 - 使用；0 - 没有使用； 负数 - 无效 */
};
