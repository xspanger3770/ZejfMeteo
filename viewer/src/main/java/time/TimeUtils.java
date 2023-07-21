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

    public static void moveBy(Calendar calendar, int samplesPerHour, int steps){
        int secondsPerStep = 3600 / samplesPerHour;
        calendar.add(Calendar.SECOND, secondsPerStep * steps);
    }

    public static int getSampleNumber(Calendar calendar, int samplesPerHour){
        int secs = calendar.get(Calendar.SECOND) + 60 * calendar.get(Calendar.MINUTE);
        return (secs * samplesPerHour) / 3600;
    }

    public static void round(Calendar calendar, int samplesPerHour) {
        int sample = getSampleNumber(calendar, samplesPerHour);
        calendar.set(Calendar.MINUTE, 0);
        calendar.set(Calendar.SECOND, 0);
        calendar.set(Calendar.MILLISECOND, 0);
        moveBy(calendar, samplesPerHour, sample);
    }
}
