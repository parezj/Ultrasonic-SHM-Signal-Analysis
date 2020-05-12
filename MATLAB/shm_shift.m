%% read csv
path = "data/";
prefix = "data";

data = readtable(path + prefix + ".csv");
durations = readtable(path + prefix + "_durations.csv");
local_max = readtable(path + prefix + "_local_max.csv");
local_min = readtable(path + prefix + "_local_min.csv");
local_cntr = readtable(path + prefix + "_local_ext_centers.csv");

interp_pli = readtable(path + prefix + "_interpolated_pli.csv");
interp_csi = readtable(path + prefix + "_interpolated_csi.csv");
interp_eci = readtable(path + prefix + "_interpolated_eci.csv");

results_pli = readtable(path + prefix + "_results_pli.csv");
results_csi = readtable(path + prefix + "_results_csi.csv");
results_eci = readtable(path + prefix + "_results_eci.csv");

%% summary
summary(data)

%% colors
c = containers.Map
c('black')   = [0,      0,      0     ];
c('blue')    = [0,      0.4470, 0.7410];
c('orange')  = [0.8500, 0.3250, 0.0980];
c('yellow')  = [0.9290, 0.6940, 0.1250];
c('magenta') = [0.4940, 0.1840, 0.5560];
c('green')   = [0.4660, 0.6740, 0.1880];
c('cyan')    = [0.3010, 0.7450, 0.9330];
c('red')     = [0.6350, 0.0780, 0.1840];
c('white')   = [1,      1,      1     ];

%% plot
for i = 1:size(data,2)
    figure;
    plot([1:size(data,1)], data.(i), '-', 'color', c('blue'));
    
    name = data.Properties.VariableNames(i);
    title(string(replace(name, '_', '_{')) + "}");
    xlabel('time');
    ylabel('value');
    grid on;
    hold on;
    
    y = i * 2;
    x = y - 1;
    z = ((i - 1) * 5) + 1;
    
    plot(local_max.(x), local_max.(y), '*', 'color', c('red'));
    plot(local_min.(x), local_min.(y), '+', 'color', c('red'));
    plot(local_cntr.(x), local_cntr.(y), 'x', 'color', c('red'));

    plot(interp_pli.(x), interp_pli.(y), '-', 'color', c('black'));
    plot(interp_csi.(x), interp_csi.(y), '-', 'color', c('orange'));
    plot(interp_eci.(x), interp_eci.(y), '-', 'color', c('green'));
    
    eci_skip_lmax = 2;
    
    lmax = local_max.(x);
    eci_x = interp_eci.(x);
    eci_y = interp_eci.(y);
    eci_x2 = eci_x(eci_x > lmax(eci_skip_lmax));
    eci_y2 = eci_y(eci_x > lmax(eci_skip_lmax));
    eci_gmax2_y = max(eci_y2);
    eci_gmax2_x = eci_x2(find(eci_y2 == eci_gmax2_y));
     
    plot(results_pli.(z), results_pli.(z + 1), 'd', 'color', c('black'));
    plot(results_csi.(z), results_csi.(z + 1), 's', 'color', c('orange'));
    plot(results_eci.(z), results_eci.(z + 1), '^', 'color', c('green'));
    plot(eci_gmax2_x(1), eci_gmax2_y, 'o', 'color', c('green'));
    
    xline(results_pli.(z), ':', 'color', c('black'));
    xline(results_csi.(z), '--', 'color', c('orange'));
    xline(results_eci.(z), '-.', 'color', c('green'));
    xline(eci_gmax2_x(1), ':', 'color', c('green'));

    xlim([0, size(data,1)]);
    ylim([-2000 2000]);
    legend('data', 'local max', 'local min', 'local extreme center', ...
        'PLI', 'CSI', 'ECI', 'PLI global max', 'CSI global max', ...
        'ECI global max 1', 'ECI global max 2', 'Location', 'Best');
    
    set(gcf, 'units','normalized','outerposition',[0 0 1 1]);
    saveas(gcf, "plot_" + name + ".png");
end
