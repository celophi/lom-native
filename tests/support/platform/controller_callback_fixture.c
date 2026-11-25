static unsigned calls;

/**
 * @brief A vertical blank callback that counts its calls, standing in for one
 *        installed before the controllers'.
 */
void lom_test_controller_prior_callback(void)
{
    ++calls;
}

/**
 * @brief Count the calls to lom_test_controller_prior_callback.
 * @return The count.
 */
unsigned lom_test_controller_prior_callback_calls(void)
{
    return calls;
}
