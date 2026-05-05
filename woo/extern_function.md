# Extern Functions Imported by `std.wo`

Below is the complete list of `woostd_*` external functions imported in `std.wo`,
organized by the Woolang namespace in which they appear.

---

## `unsafe`

| Extern Symbol | Line |
|---|---|
| `woostd_return_it_self` | 3 |

## `std`

| Extern Symbol | Line |
|---|---|
| `woostd_panic` | 8 |
| `woostd_bad_function` | 14 |
| `woostd_print` | 82 |
| `woostd_input_read_i` | 100 |
| `woostd_input_read_r` | 108 |
| `woostd_input_read_s` | 116 |
| `woostd_input_readline` | 129 |
| `woostd_random_i` | 145 |
| `woostd_random_r` | 151 |
| `woostd_yield` | 156 |
| `woostd_sleep` | 158 |
| `woostd_cmdlines` | 160 |
| `woostd_host_path` | 162 |
| `woostd_is_same` | 164 |
| `woostd_make_dup` | 166 |

## `dynamic`

| Extern Symbol | Line |
|---|---|
| `woostd_serialize_dynamic` | 195 |
| `woostd_deserialize_dynamic` | 197 |

## `char`

| Extern Symbol | Line |
|---|---|
| `woostd_char_tostring` | 513 |
| `woostd_char_toupper` | 515 |
| `woostd_char_tolower` | 517 |
| `woostd_char_isspace` | 519 |
| `woostd_char_isalpha` | 521 |
| `woostd_char_isalnum` | 523 |
| `woostd_char_isnumber` | 525 |
| `woostd_char_ishex` | 527 |
| `woostd_char_isoct` | 529 |
| `woostd_char_hexnum` | 531 |
| `woostd_char_octnum` | 533 |

## `string`

| Extern Symbol | Line |
|---|---|
| `woostd_take_token` | 538 |
| `woostd_take_string` | 540 |
| `woostd_take_int` | 542 |
| `woostd_take_real` | 544 |
| `woostd_create_wchars_from_str` | 546 |
| `woostd_create_chars_from_str` | 548 |
| `woostd_get_ascii_val_from_str` | 550 |
| `woostd_str_char_len` | 552 |
| `woostd_str_byte_len` | 554 |
| `woostd_string_sub` | 556 |
| `woostd_string_sub_len` | 558 |
| `woostd_string_sub_range` | 560 |
| `woostd_string_toupper` | 562 |
| `woostd_string_tolower` | 564 |
| `woostd_string_isspace` | 566 |
| `woostd_string_isalpha` | 568 |
| `woostd_string_isalnum` | 570 |
| `woostd_string_isnumber` | 572 |
| `woostd_string_ishex` | 574 |
| `woostd_string_isoct` | 576 |
| `woostd_string_enstring` | 578 |
| `woostd_string_destring` | 580 |
| `woostd_string_beginwith` | 582 |
| `woostd_string_endwith` | 584 |
| `woostd_string_replace` | 586 |
| `woostd_string_find` | 588 |
| `woostd_string_find_from` | 590 |
| `woostd_string_rfind` | 592 |
| `woostd_string_rfind_from` | 594 |
| `woostd_string_trim` | 596 |
| `woostd_string_split` | 598 |
| `woostd_string_split_iter` | 602 |
| `woostd_string_append_char` | 610 |
| `woostd_string_append_cchar` | 616 |

## `array`

| Extern Symbol | Line |
|---|---|
| `woostd_array_create` | 626 (via `dynamic::box_variant_name`) |
| `woostd_serialize_array` | 628 |
| `woostd_deserialize_array` | 630 |
| `woostd_create_str_by_wchar` | 650 |
| `woostd_create_str_by_ascii` | 652 |
| `woostd_array_len` | 654 |
| `woostd_make_dup` | 656, 658 |
| `woostd_array_empty` | 660 |
| `woostd_array_get` | 674 (via `dynamic::unbox_variant_name`) |
| `woostd_array_get_or_default` | 676 (via `dynamic::unbox_variant_name`) |
| `woostd_array_find` | 686 (via `dynamic::box_variant_name`) |
| `woostd_array_iter_next` | 751 (via `dynamic::unbox_variant_name`) |
| `woostd_array_iter` | 754 |
| `woostd_array_connect` | 756 |
| `woostd_array_sub` | 758 |
| `woostd_array_sub_to` | 760 |
| `woostd_array_sub_range` | 762 |
| `woostd_array_front` | 764 (via `dynamic::unbox_variant_name`) |
| `woostd_array_back` | 766 (via `dynamic::unbox_variant_name`) |
| `woostd_array_front_val` | 768 (via `dynamic::unbox_variant_name`) |
| `woostd_array_back_val` | 770 (via `dynamic::unbox_variant_name`) |

## `vec`

| Extern Symbol | Line |
|---|---|
| `woostd_array_create` | 777 (via `dynamic::box_variant_name`) |
| `woostd_serialize_array` | 779 |
| `woostd_deserialize_array` | 781 |
| `woostd_create_str_by_wchar` | 783 |
| `woostd_create_str_by_ascii` | 785 |
| `woostd_array_len` | 787 |
| `woostd_make_dup` | 789, 791 |
| `woostd_array_empty` | 793 |
| `woostd_array_resize` | 795 (via `dynamic::box_variant_name`) |
| `woostd_array_shrink` | 797 |
| `woostd_array_insert` | 799 (via `dynamic::box_variant_name`) |
| `woostd_array_swap` | 801 |
| `woostd_array_copy` | 803 |
| `woostd_array_get` | 806 (via `dynamic::unbox_variant_name`) |
| `woostd_array_get_or_default` | 808 (via `dynamic::unbox_variant_name`) |
| `woostd_array_add` | 818 (via `dynamic::box_variant_name`) |
| `woostd_array_connect` | 820 |
| `woostd_array_sub` | 822 |
| `woostd_array_sub_to` | 824 |
| `woostd_array_sub_range` | 826 |
| `woostd_array_pop` | 828 (via `dynamic::unbox_variant_name`) |
| `woostd_array_dequeue` | 830 (via `dynamic::unbox_variant_name`) |
| `woostd_array_pop_val` | 832 (via `dynamic::unbox_variant_name`) |
| `woostd_array_dequeue_val` | 834 (via `dynamic::unbox_variant_name`) |
| `woostd_array_remove` | 836 |
| `woostd_array_find` | 838 (via `dynamic::box_variant_name`) |
| `woostd_array_clear` | 850 |
| `woostd_array_iter_next` | 904 (via `dynamic::unbox_variant_name`) |
| `woostd_array_iter` | 907 |
| `woostd_array_front` | 909 (via `dynamic::unbox_variant_name`) |
| `woostd_array_back` | 911 (via `dynamic::unbox_variant_name`) |
| `woostd_array_front_val` | 913 (via `dynamic::unbox_variant_name`) |
| `woostd_array_back_val` | 915 (via `dynamic::unbox_variant_name`) |

## `dict`

| Extern Symbol | Line |
|---|---|
| `woostd_serialize_map` | 924 |
| `woostd_deserialize_map` | 926 |
| `woostd_map_len` | 942 |
| `woostd_make_dup` | 944, 946 |
| `woostd_map_only_get` | 955 (via `dynamic::map_variant_name`) |
| `woostd_map_find` | 957 (via `dynamic::box_variant_name`) |
| `woostd_map_get_or_default` | 959 (via `dynamic::map_variant_name`) |
| `woostd_map_keys` | 969 |
| `woostd_map_vals` | 971 |
| `woostd_map_empty` | 973 |
| `woostd_map_iter_next` | 983 (via `dynamic::kv_unbox_variant_name`) |
| `woostd_map_iter` | 986 |

## `map`

| Extern Symbol | Line |
|---|---|
| `woostd_serialize_map` | 1020 |
| `woostd_deserialize_map` | 1022 |
| `woostd_map_create` | 1032 |
| `woostd_map_reserve` | 1034 |
| `woostd_map_set` | 1036 (via `dynamic::kv_box_variant_name`) |
| `woostd_map_len` | 1038 |
| `woostd_make_dup` | 1040, 1042 |
| `woostd_map_find` | 1051 (via `dynamic::box_variant_name`) |
| `woostd_map_only_get` | 1053 (via `dynamic::map_variant_name`) |
| `woostd_map_get_or_default` | 1055 (via `dynamic::map_variant_name`) |
| `woostd_map_get_or_set_default` | 1057 (via `dynamic::kv_box_variant_name`) |
| `woostd_map_get_or_set_default_do` | 1059 (via `dynamic::kv_box_variant_name`) |
| `woostd_map_swap` | 1069 |
| `woostd_map_copy` | 1071 |
| `woostd_map_keys` | 1074 |
| `woostd_map_vals` | 1076 |
| `woostd_map_empty` | 1078 |
| `woostd_map_remove` | 1080 (via `dynamic::box_variant_name`) |
| `woostd_map_clear` | 1082 |
| `woostd_map_iter_next` | 1086 (via `dynamic::kv_unbox_variant_name`) |
| `woostd_map_iter` | 1089 |

## `int`

| Extern Symbol | Line |
|---|---|
| `woostd_int_to_hex` | 1119 |
| `woostd_int_to_oct` | 1121 |
| `woostd_bit_or` | 1123 |
| `woostd_bit_and` | 1125 |
| `woostd_bit_xor` | 1127 |
| `woostd_bit_not` | 1129 |
| `woostd_bit_shl` | 1131 |
| `woostd_bit_shr` | 1133 |
| `woostd_bit_ashr` | 1135 |

## `gchandle`

| Extern Symbol | Line |
|---|---|
| `woostd_gchandle_close` | 1140 |

## `tuple`

| Extern Symbol | Line |
|---|---|
| `woostd_tuple_nthcdr` | 1185 |
| `woostd_tuple_cdr` | 1192 |

---

## Summary

| Namespace | Symbol Count |
|---|---|
| `unsafe` | 1 |
| `std` | 16 |
| `dynamic` | 2 |
| `char` | 11 |
| `string` | 35 |
| `array` | 19 |
| `vec` | 30 |
| `dict` | 12 |
| `map` | 24 |
| `int` | 9 |
| `gchandle` | 1 |
| `tuple` | 2 |
| **Total** | **162** |

Note: Symbols marked with "via `dynamic::*`" are resolved dynamically at compile time
via template name generation. The actual symbol name at runtime may vary depending
on type parameters (e.g. `woostd_array_create_i`, `woostd_array_create_r`, etc.).
