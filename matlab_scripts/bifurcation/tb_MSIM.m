clc; clear; close all;

GS = 8064;
num_iter = 100;

[J,N] = findJ('g4096_lattice.txt');

a = 15;
b = 7.5;
c = 0;

% One independent noise source per spin
noise_stream = arrayfun(@(k) ...
    RandStream('Threefry','Seed',k), ...
    1:N, ...
    'UniformOutput', false);

accuracy = zeros(1,1);

for k = 1:1

    J_i = J;

    maxcut = zeros(num_iter,1);

    x = zeros(N,num_iter);
    x(:,1) = sign(randn(N,1));

    decay = linspace(1,0,num_iter);

    for i = 2:num_iter
    
        % Test H0_SB
        % x(:,i) = H0_SB(x(:,i-1),J_i,N,a,b,c,decay(i-1),noise_stream,0);
    
        % Test H1_SB
        % x(:,i) = H1_SB(x(:,i-1),J_i,N,a,b,c,decay(i-1),noise_stream,0);

        % Test M0_SB
        % x(:,i) = M0_SB(x(:,i-1),J_i,N,a,b,c,decay(i-1),noise_stream,0);

        % Test M1_SB
         x(:,i) = M1_SB(x(:,i-1),J_i,N,a,b,c,decay(i-1),noise_stream);
    end

    for i = 1:num_iter
        cut_edges = (x(:,i))*transpose(x(:,i)) == -1;
        maxcut(i) = 0.5*sum(sum(J.*cut_edges));
    end

    plot(1:num_iter,maxcut)

    accuracy(k) = 100*maxcut(end)/GS;
    disp(accuracy(k))
    disp(maxcut(end))

end

avg_accuracy = mean(accuracy);
max_accuracy = max(accuracy);
std_accuracy = std(accuracy);
sr_90 = sum(accuracy>=90)/20;
sr_95 = sum(accuracy>=95)/20;
sr_100 = sum(accuracy>=100)/20;