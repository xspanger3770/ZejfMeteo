package ui;

import data.computation.ComputationStatus;
import data.computation.ComputedLog;
import data.computation.VariableComputation;
import main.ZejfMeteo;
import time.TimeUtils;

import javax.swing.*;
import java.awt.*;
import java.awt.geom.Line2D;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

public class GraphCanvasPanel extends JPanel {
    private final SimpleGraphsPanel simpleGraphsPanel;
    private final Font calibri14 = new Font("Calibri", Font.BOLD, 14);
    private final Font calibri12 = new Font("Calibri", Font.BOLD, 12);

    public GraphCanvasPanel(SimpleGraphsPanel simpleGraphsPanel){
        this.simpleGraphsPanel=simpleGraphsPanel;
        setBackground(Color.white);
        setPreferredSize(new Dimension(600, 400));
        setBorder(BorderFactory.createLineBorder(Color.black));
    }

    public int getDurationMinutes(){
        return 30;
    }

    private int wrx, wry;
    private int w, h;
    private double max, min;
    private Calendar now, end;
    private static final Stroke dashed = new BasicStroke(1, BasicStroke.CAP_BUTT, BasicStroke.JOIN_BEVEL,
            0, new float[]{2}, 0);

    private static final DateFormat dateFormatDate = new SimpleDateFormat("dd.MM.yyyy");
    private static final DateFormat dateFormatTime = new SimpleDateFormat("HH:mm");
    private static final DateFormat dateFormatTimeSec = new SimpleDateFormat("HH:mm:ss");
    private static final int MIN_DISTANCE_SMALL = 20;
    private static final int MIN_DISTANCE_BIG = 100;

    @Override
    public void paint(Graphics gr) {
        super.paint(gr);
        Graphics2D g = (Graphics2D) gr;

        w = getWidth();
        h = getHeight();

        g.setColor(Color.white);
        g.fillRect(0,0, w, h);
        g.setColor(Color.black);
        g.drawRect(0,0,w, h);

        VariableComputation computation = simpleGraphsPanel.getSelectedVariable();
        if(computation == null){
            return;
        }

        now = Calendar.getInstance();
        now.setTime(new Date());

        end = (Calendar) now.clone();
        end.add(Calendar.MINUTE, -getDurationMinutes());

        double[] minMax = findMinMax(computation, now);
        min = minMax[0];
        max = minMax[1];
        double diff = Math.max(0.01, max - min);
        min -= diff * 0.15;
        max += diff * 0.15;

        wry = 18;

        long timeRange = calculateTimeRange(now.getTimeInMillis() - end.getTimeInMillis(), w - 50, 90);

        double[] ranges = calculateRanges(diff, h - wry);
        double topSmall = Math.ceil(max / ranges[0]) * ranges[0];
        double bottomSmall = Math.floor(min / ranges[0]) * ranges[0];
        double topBig = Math.ceil(max / ranges[1]) * ranges[1];
        double bottomBig = Math.floor(min / ranges[1]) * ranges[1];

        g.setFont(calibri14);
        wrx = 4 + Math.max(g.getFontMetrics().stringWidth(String.format("%%.%df%%s".formatted(getDecimals(ranges[1])),topBig, computation.getUnits())),
                g.getFontMetrics().stringWidth(String.format("%%.%df%%s".formatted(getDecimals(ranges[1])), bottomBig, computation.getUnits())));

        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_OFF);

        g.setColor(Color.gray);
        g.drawRect(0, 0, wrx, h);
        g.drawRect(0, h - wry, w, wry);

        Calendar calendarTimeRanges = (Calendar) end.clone();
        calendarTimeRanges.set(Calendar.SECOND, (int) ((int)((calendarTimeRanges.get(Calendar.SECOND) / timeRange)) * timeRange));
        calendarTimeRanges.set(Calendar.MINUTE, (int) ((int)((calendarTimeRanges.get(Calendar.MINUTE) / Math.max(1, timeRange / 60))) * (timeRange / 60)));
        calendarTimeRanges.set(Calendar.HOUR, (int) ((int)((calendarTimeRanges.get(Calendar.HOUR) / Math.max(1, timeRange / 3600))) * (timeRange / 3600)));

        while(calendarTimeRanges.before(now)){
            double x = getX(calendarTimeRanges);
            if(x > wrx){
                g.setColor(Color.gray);
                g.setStroke(dashed);
                g.draw(new Line2D.Double(x, 0, x, h - wry));

                g.setColor(Color.black);
                g.setFont(calibri12);
                String str = (timeRange < 60 ? dateFormatTimeSec : dateFormatTime).format(calendarTimeRanges.getTime());
                int stringWidth = g.getFontMetrics().stringWidth(str);

                if(x - (double) stringWidth / 2 > wrx) {
                    g.drawString(str, (int) x - stringWidth / 2, h - wry + 14);
                }
            }
            calendarTimeRanges.add(Calendar.SECOND, (int) timeRange);
        }

        for(double val = bottomSmall; val <= topSmall; val += ranges[0]){
            double y = getY(val);

            if(y > h - wry){
                continue;
            }

            g.setColor(Color.gray);
            g.setStroke(dashed);
            g.draw(new Line2D.Double(wrx, y, w, y));
        }

        for(double val = bottomBig; val <= topBig; val += ranges[1]){
            double y = getY(val);

            if(y > h - wry){
                continue;
            }

            g.setColor(Color.black);
            g.setStroke(new BasicStroke(2f));
            g.draw(new Line2D.Double(wrx, y, w, y));

            if(y < 16 || y > h - wry - 16){
                continue;
            }

            g.setColor(Color.black);
            g.setFont(calibri14);
            g.drawString(String.format("%%.%df%%s".formatted(getDecimals(ranges[1])),val, computation.getUnits()), 2, (int)(y+5));

        }

        Calendar calendar1 = (Calendar) end.clone();
        Calendar calendar2 = (Calendar) end.clone();
        TimeUtils.round(calendar1, computation.getSamplesPerHour());
        TimeUtils.round(calendar2, computation.getSamplesPerHour());

        TimeUtils.moveBy(calendar2, computation.getSamplesPerHour(), 1);


        while(calendar1.before(now)){
            ComputedLog log1 = ZejfMeteo.getDataManager().getValue(computation, calendar1);
            ComputedLog log2 = ZejfMeteo.getDataManager().getValue(computation, calendar2);

            if(log1 != null && log2 != null && log1.getStatus()== ComputationStatus.FINISHED &&log2.getStatus()==ComputationStatus.FINISHED){
                double x1 = getX(calendar1);
                double x2 = getX(calendar2);
                double y1 = getY(log1.getValue());
                double y2 = getY(log2.getValue());

                if(x1 >= wrx && x2 >= wrx && y1 <= h-wry && y2 <= h - wry) {
                    //System.err.println(x1+", "+x2+", "+y1+", "+y2+", "+max+", "+min+", "+log1.getValue()+", "+log2.getValue());
                    g.setColor(Color.red);
                    g.setStroke(new BasicStroke(3f));
                    g.draw(new Line2D.Double(x1, y1, x2, y2));
                }
            }

            TimeUtils.moveBy(calendar1, computation.getSamplesPerHour(), 1);
            TimeUtils.moveBy(calendar2, computation.getSamplesPerHour(), 1);
        }
    }

    private static int[] stepsSmall = {1, 2, 5, 10, 15, 20, 30};
    private static int[] stepsBig = {1, 2, 4, 8, 12};
    private static long[] mulsTime = {1, 60, 60 * 60, 60 * 60 * 24, 60 * 60 * 24 * 7};

    // seconds
    private long calculateTimeRange(long diffMS, int width, int minDist) {
        double one = (diffMS / 1000.0) * ((double) minDist / width);

        for(long mul : mulsTime){
            for(int step: (mul < 60 * 60 ? stepsSmall : stepsBig)){
                long currentStep = mul * step;
                if(currentStep > one){
                    return currentStep;
                }
            }
        }

        return 0;
    }

    private double getY(double val){
        return (h - wry) * (max - val) / (max - min);
    }

    private double getX(Calendar calendar){
        return wrx + (w - wrx) * (calendar.getTimeInMillis() - end.getTimeInMillis()) / (double)(now.getTimeInMillis() - end.getTimeInMillis());
    }

    private int getDecimals(double diff){
        return (int) Math.max(1, -Math.floor(Math.log10(diff)));
    }

    private double[] findMinMax(VariableComputation computation, Calendar now) {
        double min = 999999;
        double max = -min;

        Calendar end = (Calendar) now.clone();
        end.add(Calendar.MINUTE, -getDurationMinutes());

        Calendar calendar = (Calendar) now.clone();
        TimeUtils.moveBy(calendar, computation.getSamplesPerHour(), 1);
        while(calendar.after(end)){
            ComputedLog log = ZejfMeteo.getDataManager().getValue(computation, calendar);
            if(log != null && log.getStatus() == ComputationStatus.FINISHED){
                double val = log.getValue();
                if(val > max){
                    max = val;
                }
                if(val < min){
                    min = val;
                }
            }
            TimeUtils.moveBy(calendar, computation.getSamplesPerHour(), -1);
        }


        return new double[] {min, max};
    }

    public static double calculateRange(double diff, int h, int minDistance){
        double one = diff * ((double) minDistance / h);
        double closest = Math.pow(10, Math.floor(Math.log10(one)));

        return closest > one ? closest : closest * 2 > one ? closest * 2 : closest * 5 > one ? closest * 5 : closest * 10;
    }

    public static double[] calculateRanges(double diff, int h) {
        return new double[]{calculateRange(diff, h, MIN_DISTANCE_SMALL), calculateRange(diff, h, MIN_DISTANCE_BIG)};
    }
}