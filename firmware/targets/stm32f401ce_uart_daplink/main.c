#include "stm32f401xc.h"

void __libc_init_array(void) {
}

static void gpio_set_output(GPIO_TypeDef *gpio, uint32_t pin) {
    const uint32_t shift = pin * 2u;
    gpio->MODER &= ~(0x3u << shift);
    gpio->MODER |= (0x1u << shift);
    gpio->OTYPER &= ~(1u << pin);
    gpio->OSPEEDR |= (0x3u << shift);
    gpio->PUPDR &= ~(0x3u << shift);
}

static void gpio_set_af(GPIO_TypeDef *gpio, uint32_t pin, uint32_t af) {
    const uint32_t shift = pin * 2u;
    const uint32_t index = pin >> 3;
    const uint32_t afr_shift = (pin & 0x7u) * 4u;
    gpio->MODER &= ~(0x3u << shift);
    gpio->MODER |= (0x2u << shift);
    gpio->OTYPER &= ~(1u << pin);
    gpio->OSPEEDR |= (0x3u << shift);
    gpio->PUPDR &= ~(0x3u << shift);
    gpio->AFR[index] &= ~(0xFu << afr_shift);
    gpio->AFR[index] |= (af << afr_shift);
}

static void systick_init_1khz(void) {
    SysTick->LOAD = 16000u - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static uint8_t systick_poll_1ms(void) {
    return (uint8_t)((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0u);
}

static void usart1_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    gpio_set_output(GPIOC, 13u);
    gpio_set_af(GPIOA, 9u, 7u);
    gpio_set_af(GPIOA, 10u, 7u);

    USART1->BRR = 139u; /* 16 MHz / 115200 */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void usart1_write_byte(uint8_t value) {
    while ((USART1->SR & USART_SR_TXE) == 0u) {
    }
    USART1->DR = value;
}

static void usart1_write_text(const char *text) {
    while (*text != '\0') {
        usart1_write_byte((uint8_t)*text++);
    }
}

static uint8_t usart1_try_read_byte(uint8_t *out) {
    if ((USART1->SR & USART_SR_RXNE) == 0u) {
        return 0u;
    }
    *out = (uint8_t)USART1->DR;
    return 1u;
}

static uint8_t streq(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return 0u;
        }
        lhs++;
        rhs++;
    }
    return (uint8_t)(*lhs == '\0' && *rhs == '\0');
}

int main(void) {
    char line[64];
    uint32_t led_ms = 0u;
    uint32_t banner_ms = 0u;
    uint32_t len = 0u;

    usart1_init();
    systick_init_1khz();
    GPIOC->ODR |= (1u << 13);
    usart1_write_text("STM32F401 UART READY\r\n");

    while (1) {
        uint8_t ch = 0u;

        if (systick_poll_1ms() != 0u) {
            banner_ms += 1u;
            led_ms += 1u;
            if (led_ms >= 500u) {
                led_ms = 0u;
                GPIOC->ODR ^= (1u << 13);
            }
            if (banner_ms >= 5000u) {
                banner_ms = 0u;
                usart1_write_text("READY\r\n");
            }
        }

        if (usart1_try_read_byte(&ch) == 0u) {
            continue;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[len] = '\0';
            if (streq(line, "PING")) {
                usart1_write_text("PONG\r\n");
            } else if (line[0] == 'E' && line[1] == 'C' && line[2] == 'H' && line[3] == 'O' && line[4] == ' ') {
                usart1_write_text("ECHO ");
                usart1_write_text(&line[5]);
                usart1_write_text("\r\n");
            } else {
                usart1_write_text("ERR\r\n");
            }
            len = 0u;
            continue;
        }

        if (len + 1u < sizeof(line)) {
            line[len++] = (char)ch;
        } else {
            len = 0u;
            usart1_write_text("ERR\r\n");
        }
    }
}
