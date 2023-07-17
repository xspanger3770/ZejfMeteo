package ui;

import data.computation.ComputationStatus;
import data.computation.ComputedLog;
import data.computation.VariableComputation;
import main.ZejfMeteo;
import time.TimeUtils;

import javax.swing.*;
import java.awt.*;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;
import java.awt.geom.Line2D;
import java.util.Calendar;
import java.util.Date;
import java.util.Timer;
import java.util.TimerTask;

public class SimpleGraphsPanel extends JPanel {

    private JComboBox<VariableComputation> variableSelector;

    public SimpleGraphsPanel(){
        setLayout(new BorderLayout());
        GridLayout gridLayout = new GridLayout(2,2);
        gridLayout.setHgap(5);
        gridLayout.setVgap(5);
        JPanel configPanel = new JPanel(gridLayout);
        configPanel.add(new JLabel("Variable: "));
        configPanel.add(createSelector());
        configPanel.add(new JLabel("Duration:"));
        configPanel.add(new JButton("TBD"));

        add(configPanel, BorderLayout.PAGE_START);

        JPanel graphPanel = new GraphCanvasPanel(this);

        add(graphPanel, BorderLayout.CENTER);

        java.util.Timer timer = new Timer();

        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                SimpleGraphsPanel.super.repaint();
            }
        };

        // Schedule the task to run every two minutes, starting from now
        timer.schedule(task, 1000, 1000);
    }

    public VariableComputation getSelectedVariable(){
        return (VariableComputation) variableSelector.getSelectedItem();
    }

    private JComboBox<VariableComputation> createSelector() {
        if(variableSelector == null) {
            variableSelector = new JComboBox<>();
            variableSelector.setRenderer(new ItemListRenderer());
        }

        variableSelector.addItemListener(new ItemListener() {
            @Override
            public void itemStateChanged(ItemEvent itemEvent) {
                SimpleGraphsPanel.super.repaint();
            }
        });

        updateSelector();

        return variableSelector;
    }

    private void updateSelector() {
        variableSelector.removeAllItems();
        for(VariableComputation variableComputation : ZejfMeteo.getDataManager().getVariableCalculations()){
            variableSelector.addItem(variableComputation);
        }
    }

    private static class ItemListRenderer extends DefaultListCellRenderer {
        @Override
        public Component getListCellRendererComponent(JList<?> list, Object value, int index,
                                                      boolean isSelected, boolean cellHasFocus) {
            super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);
            if (value instanceof VariableComputation item) {
                setText(item.getDisplayName());
            }
            return this;
        }
    }

    private static class GraphCanvasPanel extends JPanel {
        private final SimpleGraphsPanel simpleGraphsPanel;

        private static final int DURATION_SECONDS = 60 * 30;
        private final Font calibri = new Font("Calibri", Font.BOLD, 14);

        private GraphCanvasPanel(SimpleGraphsPanel simpleGraphsPanel){
            this.simpleGraphsPanel=simpleGraphsPanel;
            setBackground(Color.white);
            setPreferredSize(new Dimension(600, 400));
            setBorder(BorderFactory.createLineBorder(Color.black));
        }

        private int wrx = 30;
        private int w = 0;
        private int h = 0;

        private double max, min;

        private Calendar now, end;
        private static final Stroke dashed = new BasicStroke(1, BasicStroke.CAP_BUTT, BasicStroke.JOIN_BEVEL,
                0, new float[]{2}, 0);

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
            end.add(Calendar.SECOND, -DURATION_SECONDS);

            double[] minMax = findMinMax(computation, now);
            min = minMax[0];
            max = minMax[1];
            double diff = max - min;
            min -= diff * 0.15;
            max += diff * 0.15;

            double[] ranges = calculateRanges(diff, h);
            double topSmall = Math.ceil(max / ranges[0]) * ranges[0];
            double bottomSmall = Math.floor(min / ranges[0]) * ranges[0];
            double topBig = Math.ceil(max / ranges[1]) * ranges[1];
            double bottomBig = Math.floor(min / ranges[1]) * ranges[1];

            g.setFont(calibri);
            wrx = 4 + Math.max(g.getFontMetrics().stringWidth(String.format("%%.%df%%s".formatted(getDecimals(ranges[1])),topBig, computation.getUnits())),
                    g.getFontMetrics().stringWidth(String.format("%%.%df%%s".formatted(getDecimals(ranges[1])), bottomBig, computation.getUnits())));

            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);

            g.setColor(Color.gray);
            g.drawRect(0, 0, wrx, h);

            for(double val = bottomSmall; val <= topSmall; val += ranges[0]){
                double y = getY(val);

                g.setColor(Color.gray);
                g.setStroke(dashed);
                g.draw(new Line2D.Double(wrx, y, w, y));
            }

            for(double val = bottomBig; val <= topBig; val += ranges[1]){
                double y = getY(val);

                g.setColor(Color.black);
                g.setFont(calibri);
                g.drawString(String.format("%%.%df%%s".formatted(getDecimals(ranges[1])),val, computation.getUnits()), 2, (int)(y+5));

                g.setColor(Color.black);
                g.setStroke(new BasicStroke(2f));
                g.draw(new Line2D.Double(wrx, y, w, y));
            }

            Calendar calendar1 = (Calendar) end.clone();
            Calendar calendar2 = (Calendar) end.clone();
            TimeUtils.moveBy(calendar2, computation.getSamplesPerHour(), 1);

            while(calendar1.before(now)){
                ComputedLog log1 = ZejfMeteo.getDataManager().getValue(computation, calendar1);
                ComputedLog log2 = ZejfMeteo.getDataManager().getValue(computation, calendar2);

                if(log1 != null && log2 != null && log1.getStatus()==ComputationStatus.FINISHED &&log2.getStatus()==ComputationStatus.FINISHED){
                    double x1 = getX(calendar1);
                    double x2 = getX(calendar2);
                    double y1 = getY(log1.getValue());
                    double y2 = getY(log2.getValue());
                    //System.err.println(x1+", "+x2+", "+y1+", "+y2+", "+max+", "+min+", "+log1.getValue()+", "+log2.getValue());
                    g.setColor(Color.red);
                    g.setStroke(new BasicStroke(2f));
                    g.draw(new Line2D.Double(x1, y1, x2, y2));
                }

                TimeUtils.moveBy(calendar1, computation.getSamplesPerHour(), 1);
                TimeUtils.moveBy(calendar2, computation.getSamplesPerHour(), 1);
            }
        }

        private double getY(double val){
            return  h * (max - val) / (max - min);
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
            end.add(Calendar.SECOND, -DURATION_SECONDS);

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
    }

    private static final int MIN_DISTANCE_SMALL = 30;
    private static final int MIN_DISTANCE_BIG = 150;

    public static double calculateRange(double diff, int h, int minDistance){
        double one = diff * ((double) minDistance / h);
        double closest = Math.pow(10, Math.floor(Math.log10(one)));

        return closest * 5 <= one ? closest * 5 : closest * 2 <= one ? closest * 2 : closest;
    }

    public static double[] calculateRanges(double diff, int h) {
        return new double[]{calculateRange(diff, h, MIN_DISTANCE_SMALL), calculateRange(diff, h, MIN_DISTANCE_BIG)};
    }
}
