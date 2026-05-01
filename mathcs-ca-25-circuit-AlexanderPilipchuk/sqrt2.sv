module sqrt2(
    inout   wire[15:0] IO_DATA,
    output  wire IS_NAN,
    output  wire IS_PINF,
    output  wire IS_NINF,
    output  wire RESULT,
    input   wire CLK,
    input   wire ENABLE
); 

    // Параметры для специальных значений
    parameter CONST_PZERO = 16'h0000;
    parameter CONST_NZERO = 16'h8000;
    parameter CONST_QNAN = 16'hFE00;
    parameter CONST_PINF = 16'h7C00;
    
    // Выходные флаги и состояние
    reg is_nan_flag = 0;
    reg is_pinf_flag = 0;
    reg is_ninf_flag = 0;
    reg calculation_done = 0;
    reg[15:0] output_data_reg = 16'bz; // Регистр для IO_DATA (с 'z' для inout)

    // Назначение выходов
    assign IS_NAN = is_nan_flag;
    assign IS_PINF = is_pinf_flag;
    assign IS_NINF = is_ninf_flag;
    assign RESULT = calculation_done;
    assign IO_DATA = output_data_reg; // Управляем шиной IO_DATA

    // Внутренние регистры для разбора FP числа
    reg in_sign = 0;
    reg[4:0] in_exponent = 0;
    reg[11:0] in_mantissa = 0; // 12 бит для хранения 10 бит + неявной 1 и сдвигов

    // Регистры для алгоритма вычисления
    reg[4:0] cycle_counter = 0;
    reg[3:0] leading_one_pos = 0;
    reg[3:0] shift_amount = 0;
    reg[23:0] remainder = 0;     
    reg[23:0] current_root = 0;    

    // Логика сброса (по отрицательному фронту ENABLE)
    always @(negedge ENABLE) begin
        is_nan_flag = 0;
        is_pinf_flag = 0;
        is_ninf_flag = 0; // Всегда ноль, т.к. sqrt(x) не может быть -inf
        output_data_reg = 16'bz; // Переводим шину в Z-состояние (чтение)
        calculation_done = 0;

        // Сброс компонентов FP
        in_sign = 0;
        in_exponent = 0;
        in_mantissa = 0;

        // Сброс состояния алгоритма
        cycle_counter = 0;
        leading_one_pos = 0;
        shift_amount = 0;
        remainder = 0;
        current_root = 0;
    end

    // Основная логика (по положительному фронту CLK)
    always @(posedge CLK) begin
        // 1. Счетчик циклов (работает пока ENABLE=1)
        if ((cycle_counter < 12) && (ENABLE == 1)) begin
            cycle_counter = cycle_counter + 1;
        end

        // 2. Цикл 1: Захват входных данных с шины
        if ((cycle_counter == 1) && (ENABLE == 1) && (calculation_done != 1)) begin
            in_sign = IO_DATA[15];
            in_exponent = IO_DATA[14:10];
            in_mantissa = IO_DATA[9:0]; // Младшие 10 бит, [11:10] = 0
        end

        // 3. Цикл 2: Обработка специальных случаев и нормализация
        if ((cycle_counter == 2) && (ENABLE == 1) && (calculation_done != 1)) begin 
            
            // Случай 1: +Ноль или -Ноль
            if ((in_mantissa[9:0] == 0) && (in_exponent == 0)) begin
                calculation_done = 1;
                if (in_sign == 1) output_data_reg = CONST_NZERO;
                if (in_sign == 0) output_data_reg = CONST_PZERO;
            end
            
            // Случай 2: Вход - NaN
            else if ((in_exponent == 31) && (in_mantissa[9:0] > 0)) begin
                calculation_done = 1;
                is_nan_flag = 1;
                output_data_reg[15] = in_sign;
                output_data_reg[14:10] = in_exponent;
                output_data_reg[9] = 1; // Делаем тихий NaN (QNaN)
                output_data_reg[8:0] = in_mantissa[8:0];
            end
            
            // Случай 3: Вход - Отрицательное число (не ноль)
            else if (in_sign == 1) begin
                calculation_done = 1;
                is_nan_flag = 1;
                output_data_reg = CONST_QNAN; // Выдаем наш QNaN
            end
            
            // Случай 4: Вход - Положительная бесконечность
            else if (in_exponent == 31) begin
                calculation_done = 1;
                is_pinf_flag = 1;
                output_data_reg = CONST_PINF;
            end
            
            // Случай 5: Денормализованное число
            else if ((in_mantissa[9:0] != 0) && (in_exponent == 0)) begin
                // Ищем позицию старшей '1' по полной внутренней мантиссе.
                // Это согласовано с тем, что далее алгоритм работает именно с 12-битным регистром in_mantissa.
                casez (in_mantissa)
                    12'b1???????????: leading_one_pos = 11;
                    12'b01??????????: leading_one_pos = 10;
                    12'b001?????????: leading_one_pos = 9;
                    12'b0001????????: leading_one_pos = 8;
                    12'b00001???????: leading_one_pos = 7;
                    12'b000001??????: leading_one_pos = 6;
                    12'b0000001?????: leading_one_pos = 5;
                    12'b00000001????: leading_one_pos = 4;
                    12'b000000001???: leading_one_pos = 3;
                    12'b0000000001??: leading_one_pos = 2;
                    12'b00000000001?: leading_one_pos = 1;
                    12'b000000000001: leading_one_pos = 0;
                    default:          leading_one_pos = 0; // Не должно случиться
                endcase

                // Сдвигаем старшую '1' в рабочий разряд
                shift_amount = 10 - leading_one_pos;
                in_mantissa = in_mantissa << shift_amount;

                // Для корректного деления порядка на 2 суммарный сдвиг должен быть четным
                if (shift_amount % 2 != 0) begin
                    in_mantissa = in_mantissa << 1;
                    shift_amount = shift_amount + 1;
                end

                in_exponent = 8 - shift_amount / 2; 
            end
            
            // Случай 6: Нормализованное число
            else begin
                in_mantissa[10] = 1; // Добавляем неявную '1'
                
                // Если экспонента (E-15) нечетная, делаем ее четной
                if (in_exponent % 2 == 0) begin // (E-15) нечетная
                    in_exponent = in_exponent - 1;
                    in_mantissa = in_mantissa << 1; // * 2 (компенсируем мантиссу)
                end
                
                in_exponent = (in_exponent + 15) / 2;
            end
        end

        // 4. Циклы 2-12: Итеративное вычисление (алгоритм "в столбик")
        // (Начинает на 2-м такте с уже нормализованными данными)
        if ((cycle_counter > 1) && (ENABLE == 1) && (calculation_done != 1)) begin
            
            // Сдвигаем остаток и добавляем 2 бита из мантиссы
            remainder = (remainder << 2) | (in_mantissa[11:10]);
            // Сдвигаем мантиссу, "потребляя" 2 бита
            in_mantissa = in_mantissa << 2; 
            
            // Ядро алгоритма non-restoring sqrt
            if (remainder >= (current_root << 2) + 1) begin
                remainder = remainder - ((current_root << 2) + 1);
                current_root = (current_root << 1) + 1;
            end
            else begin
                current_root = (current_root << 1);
            end

            // Завершение на 12-м такте
            if (cycle_counter == 12) begin
                calculation_done = 1;
            end

            // Выводим результат на шину
            output_data_reg[15] = 0; // Знак всегда 0
            output_data_reg[14:10] = in_exponent;
            // Корень [10:0], где [10] - неявная '1', [9:0] - мантисса
            output_data_reg[9:0] = current_root[9:0];
        end 
    end

endmodule
