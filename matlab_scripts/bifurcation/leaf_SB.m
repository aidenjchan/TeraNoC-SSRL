function [x_o] = leaf_SB(x_i,J_i,start,stop,a,b,c,decay,noise_stream)

    % Self feedback
    state = a*x_i(start:stop);

    % Coupling
    coupling = -b*J_i*x_i;

    % Independent noise per spin
    noise = zeros(stop-start+1,1);
    for k = 1:(stop-start+1)
        noise(k) = decay*c*randn(noise_stream{k},1);

    end

    % Threshold
    x_o = sign(state + coupling + noise);

end