package time;

import java.util.Calendar;

public class TimeUtils {

    public static long getHourNumber(Calendar cal){
        return (cal.getTimeInMillis()) / (1000 * 60 * 60L);
    }

    public static Calendar toCalendar(long hourNumber) {
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(hourNumber * (1000 * 60 * 60L));
        return calendar;
    }
}
