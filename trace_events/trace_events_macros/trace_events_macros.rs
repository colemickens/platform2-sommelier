// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern crate proc_macro;

use std::collections::BTreeSet;

use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, parse_quote, Ident, ItemFn, Lit, Meta, MetaNameValue, NestedMeta};

#[derive(Default)]
struct CustomAttributes {
    category: Option<Lit>,
    name: Option<Lit>,
    thread_scope: bool,
    process_scope: bool,
    global_scope: bool,
    anchor_begin: bool,
    anchor_end: bool,
    meta_idents: BTreeSet<String>,
}

impl CustomAttributes {
    // Returns true if any scope has been defined.
    fn is_scope_defined(&self) -> bool {
        self.thread_scope | self.process_scope | self.global_scope
    }

    // Returns true if the scope gets defined, false otherwise. Panics if the scope gets set, but
    // another scope was already defined.
    fn set_scope(&mut self, scope: &Ident) -> syn::Result<bool> {
        let was_defined = self.is_scope_defined();
        match scope.to_string().as_str() {
            "thread" => self.thread_scope = true,
            "process" => self.process_scope = true,
            "global" => self.global_scope = true,
            _ => return Ok(false),
        }
        if was_defined {
            return Err(syn::Error::new_spanned(
                scope,
                "scope has already been defined",
            ));
        }
        Ok(true)
    }

    // Returns true if any anchor has been defined.
    fn is_anchor_defined(&self) -> bool {
        self.anchor_begin | self.anchor_end
    }

    // Returns true if the anchor gets defined, false otherwise. Panics if the anchor gets set, but
    // another anchor was already defined.
    fn set_anchor(&mut self, anchor: &Ident) -> syn::Result<bool> {
        let was_defined = self.is_anchor_defined();
        match anchor.to_string().as_str() {
            "begin" => self.anchor_begin = true,
            "end" => self.anchor_end = true,
            _ => return Ok(false),
        }
        if was_defined {
            return Err(syn::Error::new_spanned(
                anchor,
                "anchor has already been defined",
            ));
        }
        Ok(true)
    }
}

fn parse_attributes(nested_meta_list: Vec<NestedMeta>) -> syn::Result<CustomAttributes> {
    let mut attributes = CustomAttributes::default();
    for nested_meta in nested_meta_list {
        match nested_meta {
            NestedMeta::Meta(meta) => match meta {
                Meta::NameValue(MetaNameValue { ident, lit, .. }) => {
                    match ident.to_string().as_str() {
                        "category" => {
                            if attributes.category.is_some() {
                                return Err(syn::Error::new_spanned(
                                    ident,
                                    "multiple values for `category`",
                                ));
                            }
                            attributes.category = Some(lit);
                        }
                        "name" => {
                            if attributes.name.is_some() {
                                return Err(syn::Error::new_spanned(
                                    ident,
                                    "multiple values for `name`",
                                ));
                            }
                            attributes.name = Some(lit);
                        }

                        _ => {
                            return Err(syn::Error::new_spanned(
                                ident,
                                "unrecognized attribute parameter",
                            ));
                        }
                    }
                }
                Meta::Word(ident) => {
                    if !attributes.meta_idents.insert(ident.to_string()) {
                        return Err(syn::Error::new_spanned(
                            ident,
                            "attribute identifier repeated multiple times",
                        ));
                    }
                    if attributes.set_scope(&ident)? {
                        continue;
                    }
                    if attributes.set_anchor(&ident)? {
                        continue;
                    }
                    return Err(syn::Error::new_spanned(
                        ident,
                        "unrecognized attribute identifier",
                    ));
                }
                Meta::List(list) => {
                    return Err(syn::Error::new_spanned(list, "unrecognized meta list"));
                }
            },
            NestedMeta::Literal(lit) => {
                return Err(syn::Error::new_spanned(
                    lit,
                    "unexpected literal in attribute",
                ));
            }
        }
    }
    Ok(attributes)
}

/// Writes an instant trace event to the global tracer when the function is called.
///
/// By default, the category will be `module_path!()`, the name will be the name of the decorated
/// function, and the scope will be `Thread`. Use the `category` or `name` attribute parameters to
/// customize the trace event's label. Use the word `thread`, `process`, or `global` to explicitly
/// set the scope.
///
/// Use the word `end` in the attribute parameters to write the event when the function returns
/// instead of when it is called.
///
/// # Example
///
/// ```
/// use trace_events::instant;
///
/// #[instant]
/// fn instant_event() {}
///
/// #[instant(end)]
/// fn long_instant() {
///     // ... long running work
///     // trace event will be triggered after this line
/// }
///
/// #[instant(process)]
/// fn process_event() {}
///
/// #[instant(global)]
/// fn global_event() {}
///
/// #[instant(category = "foo", name = "bar")]
/// fn __weird_internal_name() {}
/// ```
#[proc_macro_attribute]
pub fn instant(attr: TokenStream, item: TokenStream) -> TokenStream {
    let mut function = parse_macro_input!(item as ItemFn);
    let attributes = match parse_attributes(parse_macro_input!(attr as Vec<NestedMeta>)) {
        Ok(a) => a,
        Err(e) => return e.to_compile_error().into(),
    };
    let ident = function.ident.clone();
    let name = match attributes.name {
        Some(name) => quote!(#name),
        None => quote!(stringify!(#ident)),
    };
    let category = match attributes.category {
        Some(category) => quote!(#category),
        None => quote!(module_path!()),
    };
    let scope = if attributes.thread_scope {
        quote!(trace_events::Scope::Thread)
    } else if attributes.process_scope {
        quote!(trace_events::Scope::Process)
    } else if attributes.global_scope {
        quote!(trace_events::Scope::Global)
    } else {
        quote!(Default::default())
    };
    let trace_call = quote! {
        if trace_events::is_category_enabled(#category) {
            trace_events::__private_cold_trace_instant(
                #category,
                #name,
                #scope,
                file!(),
                line!()
            );
        }
    };
    let body = function.block;
    if attributes.anchor_end {
        function.block = Box::new(parse_quote!({
            struct __TraceEventsInstant;
            impl Drop for __TraceEventsInstant {
                fn drop(&mut self) {
                    #trace_call
                }
            }
            let __trace_events_instant = __TraceEventsInstant;
            #body
        }))
    } else {
        function.block = Box::new(parse_quote!({
            #trace_call
            #body
        }));
    }
    TokenStream::from(quote!(#function))
}

/// Writes a duration trace event to the global tracer when this function returns for the time this
/// function takes to run.
///
/// By default, the category will be `module_path!()` and the name will be the name of the decorated
/// function. Use the `category` or `name` attribute parameters to customize the trace event's
/// label.
///
/// # Example
///
/// ```
/// use trace_events::duration;
///
/// #[duration]
/// fn duration_event() {}
///
/// #[duration(category = "foo", name = "bar")]
/// fn __weird_internal_name() {}
/// ```
#[proc_macro_attribute]
pub fn duration(attr: TokenStream, item: TokenStream) -> TokenStream {
    let mut function = parse_macro_input!(item as ItemFn);
    let attributes = match parse_attributes(parse_macro_input!(attr as Vec<NestedMeta>)) {
        Ok(a) => a,
        Err(e) => return e.to_compile_error().into(),
    };
    let ident = function.ident.clone();
    let name = match attributes.name {
        Some(name) => quote!(#name),
        None => quote!(stringify!(#ident)),
    };
    let category = match attributes.category {
        Some(category) => quote!(#category),
        None => quote!(module_path!()),
    };
    let body = function.block;
    function.block = Box::new(parse_quote!({
        struct __TraceEventsDuration {
            begin_ts: u64,
        }
        impl Drop for __TraceEventsDuration {
            fn drop(&mut self) {
                if trace_events::is_category_enabled(#category) {
                    trace_events::__private_cold_trace_duration(
                        #category,
                        #name,
                        self.begin_ts,
                        file!(),
                        line!()
                    );
                }
            }
        }
        let __trace_events_duration = __TraceEventsDuration {
            begin_ts: trace_events::sys::get_monotonic()
        };
        #body
    }));
    TokenStream::from(quote!(#function))
}

/// Writes a counter trace event to the global tracer when the function is called.
///
/// By default, the category will be `module_path!()` and the name will be the name of the decorated
/// function. Use the `category` or `name` attribute parameters to customize the trace event's
/// label. The name of the counter will be `"calls"`. Only calls while the category is enabled are
/// counted.
///
/// Use the word `end` in the attribute parameters to write the event when the function returns
/// instead of when it is called.
///
/// # Example
///
/// ```
/// use trace_events::counter;
///
/// #[counter]
/// fn counter_event() {}
///
/// #[counter(end)]
/// fn long_counter() {
///     // ... long running work
///     // trace event will be triggered after this line
/// }
///
/// #[counter(category = "foo", name = "bar")]
/// fn __weird_internal_name() {}
/// ```
#[proc_macro_attribute]
pub fn counter(attr: TokenStream, item: TokenStream) -> TokenStream {
    let mut function = parse_macro_input!(item as ItemFn);
    let attributes = match parse_attributes(parse_macro_input!(attr as Vec<NestedMeta>)) {
        Ok(a) => a,
        Err(e) => return e.to_compile_error().into(),
    };
    let ident = function.ident.clone();
    let name = match attributes.name {
        Some(name) => quote!(#name),
        None => quote!(stringify!(#ident)),
    };
    let category = match attributes.category {
        Some(category) => quote!(#category),
        None => quote!(module_path!()),
    };
    let trace_counter = quote! {
        static __TRACE_EVENTS_COUNTER: std::sync::atomic::AtomicUsize =
            std::sync::atomic::AtomicUsize::new(0);
        if trace_events::is_category_enabled(#category) {
            trace_events::__private_cold_trace_counter(
                #category,
                #name,
                "calls",
                (__TRACE_EVENTS_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed) + 1) as u64
            );
        }
    };
    let body = function.block;
    if attributes.anchor_end {
        function.block = Box::new(parse_quote!({
            struct __TraceEventsCounter;
            impl Drop for __TraceEventsCounter {
                fn drop(&mut self) {
                    #trace_counter
                }
            }
            let __trace_events_instant = __TraceEventsCounter;
            #body
        }))
    } else {
        function.block = Box::new(parse_quote!({
            #trace_counter
            #body
        }));
    }
    TokenStream::from(quote!(#function))
}
