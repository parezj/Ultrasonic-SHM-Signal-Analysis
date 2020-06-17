%% cfg - paths
path_in = "../data/";
path_res = "../data/ni=10/";
prefix = "data";

%% cfg - figure vertical limits
lim_offset = 50;
lim_ceil = 100;

%% read csv
data = readtable(path_in + prefix + ".csv");
durations = readtable(path_res + prefix + "_durations.csv");
local_max = readtable(path_res + prefix + "_local_max.csv");
local_min = readtable(path_res + prefix + "_local_min.csv");
local_cntr = readtable(path_res + prefix + "_local_ext_centers.csv");

interp_pli = readtable(path_res + prefix + "_interpolated_pli.csv");
interp_csi = readtable(path_res + prefix + "_interpolated_csi.csv");
interp_eci = readtable(path_res + prefix + "_interpolated_eci.csv");
interp_pli_min = readtable(path_res + prefix + "_interpolated_min_pli.csv");
interp_csi_min = readtable(path_res + prefix + "_interpolated_min_csi.csv");

results_pli = readtable(path_res + prefix + "_results_pli.csv");
results_csi = readtable(path_res + prefix + "_results_csi.csv");
results_eci = readtable(path_res + prefix + "_results_eci.csv");

%% summary
summary(data)

%% colors
c = containers.Map;
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
    break;
    % create figure
    figure;
    plot(1:size(data,1), data.(i), '-', 'color', c('blue'));
    
    % set basic properties
    name = data.Properties.VariableNames(i);
    title(string(replace(name, '_', '_{')) + "}");
    xlabel('time');
    ylabel('value');
    grid on;
    hold on;
    
    % get helper vars
    y = i * 2;
    x = y - 1;
    z = ((i - 1) * 5) + 1;
    
    % plot all main values
    plot(local_max.(x), local_max.(y), '*', 'color', c('red'));
    plot(local_min.(x), local_min.(y), '+', 'color', c('red'));
    plot(local_cntr.(x), local_cntr.(y), 'x', 'color', c('red'));
    plot(interp_pli.(x), interp_pli.(y), '-', 'color', c('black'));
    plot(interp_csi.(x), interp_csi.(y), '-', 'color', c('orange'));
    plot(interp_eci.(x), interp_eci.(y), '-', 'color', c('green'));
    plot(interp_pli_min.(x), interp_pli_min.(y), '-', 'color', c('magenta'));
    plot(interp_csi_min.(x), interp_csi_min.(y), '-', 'color', c('cyan'));
    
    % calculate additional ECI global max
    eci_skip_lmax = 2;    
    lmax = local_max.(x);
    eci_x = interp_eci.(x);
    eci_y = interp_eci.(y);
    eci_x2 = eci_x(eci_x > lmax(eci_skip_lmax));
    eci_y2 = eci_y(eci_x > lmax(eci_skip_lmax));
    eci_gmax2_y = max(eci_y2);
    eci_gmax2_x = eci_x2(eci_y2 == eci_gmax2_y);
    
    % global max points
    plot(results_pli.(z), results_pli.(z + 1), 'd', 'color', c('black'));
    plot(results_csi.(z), results_csi.(z + 1), 's', 'color', c('orange'));
    plot(results_eci.(z), results_eci.(z + 1), '^', 'color', c('green'));
    plot(eci_gmax2_x(1), eci_gmax2_y, 'o', 'color', c('green'));
    
    % vertical global max lines
    xline(results_pli.(z), ':', 'color', c('black'));
    xline(results_csi.(z), '--', 'color', c('orange'));
    xline(results_eci.(z), '-.', 'color', c('green'));
    xline(eci_gmax2_x(1), ':', 'color', c('green'));

    % set figure limits and legend
    xlim([0, size(data,1)]);
    min_val = (min(interp_csi_min.(x+1)) - lim_offset) * -1;
    max_val = max(interp_csi.(x+1)) + lim_offset;
    ylim([-(ceil(min_val/lim_ceil)*lim_ceil), ceil(max_val/lim_ceil)*lim_ceil]);
    legend('signal', 'local max', 'local min', 'local extreme center', ...
        'PLI max', 'CSI max', 'ECI', 'PLI min', 'CSI min', 'PLI global max', ...
        'CSI global max', 'ECI global max 1', 'ECI global max 2', 'Location', 'northeast');
    set(gcf, 'units','normalized','outerposition',[0 0 1 1]);
        
    % optionaly manualy adjust figure
    disp("press any key to continue...");
    pause;
    
    % finally save to png
    saveas(gcf, "plot_" + name + ".png");
    close(gcf);
end

%% plot combined signals
for i = 2:size(data,2) 
    % create figure
    figure;
    x = i * 5;
    shift_pli = results_pli.(x);
    shift_csi = results_csi.(x);
    
    % plot data
    plot(1:size(data,1), data.(1), '-', 'color', c('black'));
    hold on
    plot((1:size(data,1)) - shift_pli, data.(i), '-', 'color', c('blue'));
    plot((1:size(data,1)) - shift_csi, data.(i), '-', 'color', c('orange'));
    
    % set basic properties
    name = data.Properties.VariableNames(i);
    title(string(replace(name, '_', '_{')) + "} wrt D_{ref}");
    xlabel('time');
    ylabel('value');
    grid on;
    
    % set figure limits and legend
    xlim([0, size(data,1)]);
    ylim([-2000, 2000]);
    legend('reference signal', 'shifted signal - PLI', 'shifted signal - CSI', ...
        'Location', 'northeast');
    set(gcf, 'units','normalized','outerposition',[0 0 1 1]);
    
    % optionaly manualy adjust figure
    disp("press any key to continue...");
    pause;
    
    % finally save to png
    %saveas(gcf, "combined_" + name + ".png");
    close(gcf);
end
