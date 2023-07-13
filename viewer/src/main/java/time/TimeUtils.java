package time;

import java.util.Calendar;

public class TimeUtils {

    public static long getHourNumber(Calendar cal){
        return (cal.getTimeInMillis()) / (1000 * 60 * 60L);
    }

}
