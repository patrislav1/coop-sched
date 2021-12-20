.syntax unified
.equ EXC_RETURN, 0xfffffff9

        .section .text
        .thumb

        .extern hs_context_switch

        .global SVC_Handler
        .global PendSV_Handler

        .thumb_func
        .type PendSV_Handler, %function
PendSV_Handler:
        .fnstart
        .cantunwind

        mrs r0, msp
        mov r1, #0
        stmdb r0!, {r4-r11}

        ldr r12, =hs_context_switch
        blx r12

        ldmia r0!, {r4-r11}

        mvn lr, #~EXC_RETURN
        msr msp, r0

        bx lr

        .fnend
        .size PendSV_Handler, .-PendSV_Handler

        .thumb_func
        .type SVC_Handler, %function
SVC_Handler:
        .fnstart
        .cantunwind

        bx lr

        .fnend
        .size SVC_Handler, .-SVC_Handler

        .balign 4
        .end
