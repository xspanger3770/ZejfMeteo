package ui;

import data.computation.DirectComputation;
import data.computation.VariableComputation;
import main.ZejfMeteo;

import javax.swing.*;
import javax.swing.event.ListSelectionEvent;
import javax.swing.table.DefaultTableModel;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Arrays;

public class ComputationsManager extends JDialog {

    private final DefaultTableModel model;
    private final JTable table;
    private JButton configureButton;
    private JButton deleteButton;

    public ComputationsManager(ZejfFrame zejfFrame) {
        super(zejfFrame, true);
        setPreferredSize(new Dimension(600, 400));
        setTitle("Variables Manager");

        // Create the column names
        String[] columnNames = {
                "Variable ID", "Samples per Hour", "Display Name", "Units"
        };

        // Create a DefaultTableModel with the data and column names
        model = new DefaultTableModel(columnNames, 0);

        updateModel();

        // Create a JTable with the DefaultTableModel
        table = new JTable(model){
            @Override
            public boolean isCellEditable(int row, int column) {
                return false;
            }
        };

        table.getSelectionModel().addListSelectionListener(this::rowSelectionChanged);

        // Add the JTable to a JScrollPane
        JScrollPane scrollPane = new JScrollPane(table);

        // Add the JScrollPane to the JPanel
        add(createToolbar(), BorderLayout.PAGE_START);
        add(scrollPane, BorderLayout.CENTER);


        pack();
        setLocationRelativeTo(null);
    }

    private void updateModel() {
        int rowCount = model.getRowCount();
        for (int i = rowCount - 1; i >= 0; i--) {
            model.removeRow(i);
        }

        for (VariableComputation computation : ZejfMeteo.getDataManager().getVariableCalculations()) {
            Object[] row = {
                    computation.getVariableId(),
                    computation.getSamplesPerHour(),
                    computation.getDisplayName(),
                    computation.getUnits(),
            };
            model.addRow(row);
        }
        model.fireTableDataChanged();
    }

    private void rowSelectionChanged(ListSelectionEvent event) {
        var selectionModel = (ListSelectionModel) event.getSource();
        var count = selectionModel.getSelectedItemsCount();
        configureButton.setEnabled(count == 1);
        deleteButton.setEnabled(count >= 1);
    }

    private JToolBar createToolbar() {
        JToolBar toolbar = new JToolBar();
        JButton addButton = new JButton("Add");
        addButton.addActionListener(e -> {
            new ComputationEditor((Frame) ComputationsManager.this.getParent(), new VariableComputation(0,0,"","", new DirectComputation()));
            updateModel();
        });

        configureButton = new JButton("Configure");
        configureButton.addActionListener(actionEvent -> {
            new ComputationEditor((Frame) ComputationsManager.this.getParent(), ZejfMeteo.getDataManager().getVariableCalculations().get(table.getSelectedRow()));
            updateModel();
        });

        configureButton.setEnabled(false);

        deleteButton = new JButton("Delete");
        deleteButton.addActionListener(actionEvent -> {
            int choice = JOptionPane.showConfirmDialog(null, "Are you sure?", "Confirmation", JOptionPane.YES_NO_OPTION);

            if (choice != JOptionPane.YES_OPTION) {
                return;
            }

            int[] indicesToRemove = table.getSelectedRows();

            // Sort the indices in descending order
            Arrays.sort(indicesToRemove);
            for (int i = indicesToRemove.length - 1; i >= 0; i--) {
                int index = indicesToRemove[i];
                if (index >= 0 && index < ZejfMeteo.getDataManager().getVariableCalculations().size()) {
                    ZejfMeteo.getDataManager().getVariableCalculations().remove(index);
                }
            }

            updateModel();
        });

        deleteButton.setEnabled(false);

        toolbar.add(addButton);
        toolbar.add(configureButton);
        toolbar.add(deleteButton);

        return toolbar;
    }

}
